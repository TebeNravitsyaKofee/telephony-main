#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <nlohmann/json.hpp>
#include <vector>

#include "settings.h"

using json = nlohmann::json;

static SSL_CTX* client_ctx;
static SSL_CTX* server_ctx;

std::string hostname = "https://app.mango-office.ru/vpbx";
const char* host = "app.mango-office.ru";

bool read(int fd)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
    timeval tv{5,0};
    return select(fd+1, &fds, nullptr, nullptr, &tv) > 0;
}

bool write(int fd)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
    timeval tv{5,0};
    return select(fd+1, nullptr, &fds, nullptr, &tv) > 0;
}

void sslInit()
{
    //initializing ssl
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    const SSL_METHOD* client_method = TLS_client_method();
    client_ctx = SSL_CTX_new(client_method);

    const SSL_METHOD* server_method = TLS_server_method();
    server_ctx = SSL_CTX_new(server_method);

    //initializing client
    
}

std::string generateSignature(const std::string& key, const json& body, const std::string& salt)
{
    std::string signature = key + body.dump() + salt;
    return signature;
}

std::string sha256(const std::string& input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()),input.size(),hash);
    std::ostringstream result;
    for (unsigned char byte : hash)
        result << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    return result.str();
}

std::string sendMessage(std::string(request))
{
    //resolving mango hostname to ip
    addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, nullptr, &hints, &res);
    if (status != 0)
    {
        std::cerr << "getaddrinfo error" << gai_strerror(status) << std::endl;
    }

    //initializing client
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(client_socket <0)
    {
        perror("clientSocket");
        return "client socket error";
    }
    fcntl(client_socket , F_SETFL, O_NONBLOCK);//setting nonblocking port

    //inet pton converts ip to binary and adds it to descriptor
    //pton now commented, now we get dns name resolving, might uncomment later
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(443);
    server_addr.sin_addr = ((sockaddr_in*)res->ai_addr)->sin_addr;
    //inet_pton(AF_INET, "81.88.85.67", &server_addr.sin_addr);

    //dns resolving for debugging, not needed in code
    /*
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&server_addr.sin_addr,ip_str,sizeof(ip_str));
    std::cout << ip_str;
    */
    

    //con will give -1 and EINPROGRESS error cause of nonblocking port
    int con = connect(client_socket, (sockaddr*)&server_addr,sizeof(server_addr));
    if(con<0 && errno != EINPROGRESS)
    {
        perror("clientConnect");
        close(client_socket);
        return "client connection error";
    }

    SSL* ssl = SSL_new(client_ctx);
    SSL_set_fd(ssl,client_socket);
    SSL_set_connect_state(ssl);

    //handshake
    while(true)
    {
        //1 = end of stream
        //0 = bad
        //<0 = SSL_get_error
        int stream = SSL_connect(ssl);
        if (stream == 1)
        {
            break;
        }

        int err = SSL_get_error(ssl, stream);
        //want read and write are not fatal
        if (err == SSL_ERROR_WANT_READ)
        {
            if(!read(client_socket))
            {
                std::cerr << "Handshake timeout (read)\n";
                return "Handshake timeout (read)";
            }
        }
        else if(err ==SSL_ERROR_WANT_WRITE)
        {
            if(!write(client_socket))
            {
                std::cerr << "Handshake timeout (write)\n";
                return "Handshake timeout (write)";
            }
        }
        //everything else is fatal
        else
        {
            std::cerr << "SSL error: " << ERR_error_string(ERR_get_error(),nullptr)<<std::endl;
            //disposing garbage
            SSL_free(ssl);
            SSL_CTX_free(client_ctx);
            close(client_socket);
            return "SSL error";
        }
    }

    std::cout << "SSL connection succesfull";

    

    int sent = 0;
    int request_len = request.size();

    while(sent<request_len)
    {
        int ret = SSL_write(ssl,request.c_str() + sent,request_len-sent);
        if (ret > 0)
        {
            sent += ret;
        }
        else
        {
            int err = SSL_get_error(ssl,ret);
            //everything else except SSL_ERROR_WANT_WRITE is critical
            if (err == SSL_ERROR_WANT_WRITE)
            {
                if(!write(client_socket))
                {
                    std::cerr << "Error in SSL_write\n";
                    break;
                }
            }
            else
            {
                std::cerr << "Error during request sending\n";
                break;
            }
        }
    }

    char buffer[4096];

    while(true)
    {
        int ret = SSL_read(ssl,buffer,sizeof(buffer)-1);
        if (ret > 0)
        {
            buffer[ret]='\0';
            std::cout << buffer;
        }
        else
        {
            int err = SSL_get_error(ssl,ret);
            //everything else except SSL_ERROR_WANT_WRITE is critical
            if (err == SSL_ERROR_WANT_READ)
            {
                if(!read(client_socket))
                {
                    std::cerr << "Error in SSL_read\n";
                    break;
                }
            }
            else
            {
                std::cerr << "Error during request reading\n";
                break;
            }
        }
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    freeaddrinfo(res);

    return buffer;
}

void getLines()
{
//getting unique and salt
    std::string key = getUnique();
    std::string secret = getKey();

    //json initializing, need to initialize it as an sendMessage argument later
    json a = {};
    std::string json_str = a.dump();

    //contatinating all the data for sign generating
    //std::string gen_sign = generateSignature(key, a, secret);
    std::string to_sign = key + json_str + secret;
    //generating sha256 sign
    std::string sign = sha256(to_sign);

    char body[1024];
    snprintf(body,sizeof(body),"vpbx_api_key=%s&sign=%s&json=%s",key.c_str(), sign.c_str(), json_str.c_str());

    size_t body_len = strlen(body);

    char request[1024];   
    //s - char, zu - size_t
    snprintf(request, sizeof(request),
    "POST https://app.mango-office.ru/vpbx/config/users/request HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: %zu\r\n"
    "\r\n"
    "%s",
    hostname, body_len, body);

    std::string json_data = sendMessage(request);
    
    //method of getting json body
    std::string b = json_data.substr(json_data.find("\r\n\r\n"));

    std::vector<std::string> extensions;7
    try 
    {
        size_t start = b.find_first_of('{');
        size_t end = b.find_last_of('}');

        if (end != std::string::npos) 
        {
        b = b.substr(start, end - start +1 ); 
        }
        json j = json::parse(b);
        
        
        for (const auto& user : j["users"]) 
        {
            if (user.contains("telephony") && user["telephony"].contains("extension")) 
            {
                extensions.push_back(user["telephony"]["extension"].get<std::string>());
            }
        }
    } 
    catch (const json::parse_error& e) 
    {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        std::cerr << "Body content: " << b << std::endl;
    }
}