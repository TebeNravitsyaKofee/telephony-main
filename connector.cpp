#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
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

    int ret = select(fd+1, &fds, nullptr, nullptr, &tv);

    if (ret < 0) 
    {
        std::string error = "select() error in read() function: " + std::string(strerror(errno));
        #ifdef DEBUG
            std::cerr << error << std::endl;
        #endif
        appendLog(error);
    }
    else if (ret == 0) 
    {
        std::string error = "select() timeout in read() function: " + std::string(strerror(errno));
        #ifdef DEBUG
            std::cerr << error << std::endl;
        #endif
        appendLog(error);
    }


    return ret > 0;
}

bool write(int fd)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
    timeval tv{5,0};
    
    int ret = select(fd+1, nullptr, &fds, nullptr, &tv);
    if (ret < 0) 
    {
        std::string error = "select() error in write() function: " + std::string(strerror(errno));
        #ifdef DEBUG
            std::cerr << error << std::endl;
        #endif
        appendLog(error);
    }
    else if (ret == 0) 
    {
        std::string error = "select() timeout in write() function: " + std::string(strerror(errno));
        #ifdef DEBUG
            std::cerr << error << std::endl;
        #endif
        appendLog(error);
    }

    
    return ret > 0;
}

void sslInit()
{
    //initializing ssl
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    const SSL_METHOD* client_method = TLS_client_method();
    client_ctx = SSL_CTX_new(client_method);

    if (!client_ctx) 
    {
        std::string error = "SSL_CTX_new (client) failed: " + std::string(ERR_error_string(ERR_get_error(), nullptr));
        #ifdef DEBUG
            std::cerr << error << std::endl;
        #endif
        appendLog(error);
    }

    const SSL_METHOD* server_method = TLS_server_method();
    server_ctx = SSL_CTX_new(server_method);

    if (!server_ctx) 
    {
        std::string error = "SSL_CTX_new (server) failed: " + std::string(ERR_error_string(ERR_get_error(), nullptr));
        #ifdef DEBUG
            std::cerr << error << std::endl;
        #endif
        appendLog(error);
    } 

}

//generating signature for mango services, as their api suggests
std::string generateSignature(const std::string& key, const json& body, const std::string& salt)
{
    std::string signature = key + body.dump() + salt;
    return signature;
}

//converting data to sha256 as mango suggests
std::string sha256(const std::string& input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()),input.size(),hash);
    std::ostringstream result;
    for (unsigned char byte : hash)
        result << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    return result.str();
}


std::string sendMessage(std::string request)
{
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, "443", &hints, &res);
    if (status != 0) 
    {
        std::string error = "getaddrinfo error: " + std::string(gai_strerror(status));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        return "";
    }

    int client_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (client_socket < 0) 
    {
        std::string error = "socket() error: " + std::string(strerror(errno));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        freeaddrinfo(res);
        return "";
    }

    if (fcntl(client_socket, F_SETFL, O_NONBLOCK) == -1) 
    {
        std::string error = "fcntl(O_NONBLOCK) failed: " + std::string(strerror(errno));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        close(client_socket);
        freeaddrinfo(res);
        return "";
    }

    int con = connect(client_socket, res->ai_addr, res->ai_addrlen);
    if (con < 0 && errno != EINPROGRESS) 
    {
        std::string error = "connect() error: " + std::string(strerror(errno));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        close(client_socket);
        freeaddrinfo(res);
        return "";
    }

    //checking for socket readiness
    if (!write(client_socket)) 
    {
        std::string error = "client socket is not ready after connect()";
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        close(client_socket);
        freeaddrinfo(res);
        return "";
    }

    //getting socket error and writing it to to log
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    if (getsockopt(client_socket, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0 || so_error != 0) 
    {
        std::string error = "connect failed (SO_ERROR): " + std::string(strerror(so_error));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        close(client_socket);
        freeaddrinfo(res);
        return "";
    }

    //
    SSL* ssl = SSL_new(client_ctx);
    if (!ssl) 
    {
        std::string error = "SSL_new failed: " + std::string(ERR_error_string(ERR_get_error(), nullptr));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        close(client_socket);
        freeaddrinfo(res);
        return "";
    }

    SSL_set_tlsext_host_name(ssl, host);
    SSL_set_fd(ssl, client_socket);
    SSL_set_connect_state(ssl);

    //handshaking
    while (true) 
    {
        //1 = end of stream
        //0 = fatal error
        //<0 = SSL_get_error
        int rc = SSL_connect(ssl);
        if (rc == 1) 
        {
            break;
        }
        int err = SSL_get_error(ssl, rc);
        ///want read and watn write are not fatal and occur naturally
        if (err == SSL_ERROR_WANT_READ) 
        {
            if (!read(client_socket)) 
            { 
                appendLog("Handshake timeout (read)"); 
                goto cleanup_error; 
            }
        } 
        else if (err == SSL_ERROR_WANT_WRITE) 
        {
            if (!write(client_socket)) 
            { 
                appendLog("Handshake timeout (write)"); 
                goto cleanup_error; 
            }
        } 
        else 
        {
            std::string error = "SSL_connect error: " + std::string(ERR_error_string(ERR_get_error(), nullptr));
            appendLog(error);
            #ifdef DEBUG
            std::cerr << error << std::endl;
            #endif
            goto cleanup_error;
        }
    }

    //request sending
    {
        size_t sent = 0;
        while (sent < request.size()) 
        {
            //static cast is a safe type of conversion
            int rc = SSL_write(ssl, request.data() + sent, static_cast<int>(request.size() - sent));
            if (rc > 0) 
            {
                sent += static_cast<size_t>(rc);
                continue;
            }
            int err = SSL_get_error(ssl, rc);
            if (err == SSL_ERROR_WANT_WRITE) 
            {
                if (!write(client_socket)) { appendLog("SSL_write timeout"); goto cleanup_error; }
            } 
            else if (err == SSL_ERROR_WANT_READ) 
            {
                if (!read(client_socket)) 
                { 
                    appendLog("SSL_write WANT_READ timeout");
                    goto cleanup_error; 
                }
            } 
            else 
            {
                std::string error = "SSL_write error: " + std::string(ERR_error_string(ERR_get_error(), nullptr));
                appendLog(error);
                #ifdef DEBUG
                std::cerr << error << std::endl;
                #endif
                goto cleanup_error;
            }
        }
    }

    //reading the answer, supports chunked type
    {
        std::string response;
        std::string headers;
        std::string body;
        std::string s;
        bool headers_done = false;
        bool chunked = false;
        size_t content_length = 0;




        char buf[4096];

        //lambda that parses headers
        auto parse_headers = [&](const std::string& h) 
        {
            //Transfer-Encoding
            if (h.find("Transfer-Encoding:") != std::string::npos && h.find("chunked") != std::string::npos) 
            {
                chunked = true;
            }

            size_t p = h.find("Content-Length:");
            
            //skipping the spacebars
            while (p < h.size() && (h[p] == ' ' || h[p] == '\t')) ++p;
            size_t end = h.find("\r\n", p);
            if (end == std::string::npos) 
            {
                end = h.find('\n', p);
            }
            if (end == std::string::npos) 
            {
                appendLog("Malformed header");
                return;
            }

            std::string n = h.substr(p, end - p);
            try 
            {
                //stoull is converter from string to unsigned int
                content_length = static_cast<size_t>(std::stoull(n));
            }
            catch (...) 
            {
                content_length = 0;
            }
            
        };

        //temporary string for non-chunked input
        std::string body_raw;

        //chunk buffer
        std::string recv_buffer;
        bool chunked_complete = false;

        while (true) 
        {
            int rc = SSL_read(ssl, buf, sizeof(buf)-1);
            if (rc > 0) 
            {
                recv_buffer.append(buf, rc);
                
                //waiting for headers to arrive and be read
                if (!headers_done) 
                {
                    size_t pos = recv_buffer.find("\r\n\r\n");
                    //success
                    if (pos != std::string::npos) 
                    {
                        headers_done = true;
                        headers = recv_buffer.substr(0, pos);
                        parse_headers(headers);

                        //getting the body
                        body_raw = recv_buffer.substr(pos + 4);
                        recv_buffer.clear();
                    } 
                    else 
                    {
                        //waiting for headers
                        continue;
                    }
                }
                else 
                {
                    //if headers are present
                    body_raw.append(recv_buffer);
                    recv_buffer.clear();
                }

                //if chunked and fully arrived
                if (headers_done && chunked && !chunked_complete) 
                {
                    static std::string decoded;
                    static size_t idx = 0;

                    static std::string chunk_stream;
                    chunk_stream.append(body_raw);
                    body_raw.clear();

                    while (true) 
                    {
                        //finding chunk size
                        size_t line_end = chunk_stream.find("\r\n", idx);
                        if (line_end == std::string::npos) 
                        { 
                            break; 
                        }
                        //len_str contains the length of a chunk in hex
                        std::string len_str = chunk_stream.substr(idx, line_end - idx);
                        //removing chunk extensions just in case
                        size_t semicol = len_str.find(';');
                        if (semicol != std::string::npos) 
                        {
                            len_str = len_str.substr(0, semicol);
                        }

                        size_t chunk_size = 0;
                        try 
                        {
                            //converting from hex to size_t
                            chunk_size = std::stoul(len_str, nullptr, 16);
                        } 
                        catch (...) 
                        {
                            #ifdef DEBUG
                            std::cerr << "Chunk size parse error" << std::endl;
                            #endif
                            appendLog("Chunk size parse error");
                            goto cleanup_error;
                        }

                        //moving the cursor to the end after \r\n
                        idx = line_end + 2;

                        //checking if chunk arrived fully
                        if (chunk_stream.size() < idx + chunk_size + 2) 
                        {
                            idx -= (len_str.size() + 2);
                            break;
                        }

                        //chunk completed
                        if (chunk_size == 0) 
                        {
                            idx += 2;
                            chunked_complete = true;
                            body = decoded;
                            decoded.clear();
                            chunk_stream.clear();
                            idx = 0;
                            break;
                        }

                        //appending temporary var
                        decoded.append(chunk_stream, idx, chunk_size);
                        idx += chunk_size;

                        //waiting for the last \r\n
                        if (chunk_stream.size() < idx + 2) 
                        {
                            idx -= (len_str.size() + 2 + chunk_size);
                            break;
                        }
                        //checking for chunc completteness
                        if (chunk_stream[idx] != '\r' || chunk_stream[idx+1] != '\n') 
                        {
                            #ifdef DEBUG
                            std::cerr << "Malformed chunk: missing CRLF after data" << std::endl;
                            #endif
                            appendLog("Malformed chunk: missing CRLF after data");
                            goto cleanup_error;
                        }
                        idx += 2;
                        
                        //cleaning garbage
                        if (idx >= chunk_stream.size()) 
                        {
                            chunk_stream.clear();
                            idx = 0;
                        }
                    }
                    if (chunked_complete) 
                    {
                        break;
                    }
                }
                //for non-chunked
                else if (headers_done && !chunked && content_length > 0) 
                {
                    if (body_raw.size() >= content_length) {
                        body = body_raw.substr(0, content_length);
                        break;
                    }
                }

            }
            //error handling
            else
            {
                int err = SSL_get_error(ssl, rc);
                if (err == SSL_ERROR_WANT_READ)
                {
                    if (!read(client_socket))
                    {
                        appendLog("SSL_read timeout"); 
                        goto cleanup_error; 
                    }
                }
                //correct ssl closing
                else if (err == SSL_ERROR_ZERO_RETURN) 
                {
                    if (!headers_done) 
                    {
                        #ifdef DEBUG
                        std::cerr << "Connection closed before headers received" << std::endl;
                        #endif
                        appendLog("Connection closed before headers received");
                        goto cleanup_error;
                    }
                    if (chunked) 
                    {
                        #ifdef DEBUG
                        std::cerr << "Connection closed before parsing chunked body" << std::endl;
                        #endif
                        appendLog("Connection closed before parsing chunked body");
                        goto cleanup_error;
                    }
                    else if (content_length > 0) 
                    {
                        if (body_raw.size() < content_length) 
                        {
                            #ifdef DEBUG
                            std::cerr << "Connection closed before Content-Length satisfied" << std::endl;
                            #endif
                            appendLog("Connection closed before Content-Length satisfied");
                            goto cleanup_error;
                        }
                        body = body_raw.substr(0, content_length);
                    } 
                    //without Conent-Length
                    else 
                    {
                        body = body_raw;
                    }
                    break;
                } 
                else if (err == SSL_ERROR_WANT_WRITE) 
                {
                    if (!write(client_socket)) 
                    {
                        #ifdef DEBUG
                            std::cerr << "SSL_read WANT_WRITE timeout" << std::endl;
                        #endif
                        appendLog("SSL_read WANT_WRITE timeout"); 
                        goto cleanup_error; 
                    }
                } 
                else 
                {
                    std::string error = "SSL_read error: " + std::string(ERR_error_string(ERR_get_error(), nullptr));
                    appendLog(error);
                    #ifdef DEBUG
                    std::cerr << error << std::endl;
                    #endif
                    goto cleanup_error;
                }
            }
        }
        
        std::string full_response = headers + "\r\n\r\n" + body;
        appendLog(full_response);

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        freeaddrinfo(res);
        return full_response;
    }

    cleanup_error:
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    freeaddrinfo(res);
    return "";
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

    appendLog(request);

    std::string json_data = sendMessage(request);
    
    //method of getting json body
    std::string b = json_data.substr(json_data.find("\r\n\r\n"));

    std::map<std::string,std::string> extensions;
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
                extensions.insert({user["telephony"]["extension"].get<std::string>()+"=","off"});
            }
        }
    } 
    catch (const json::parse_error& e) 
    {
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        std::cerr << "Body content: " << b << std::endl;

        appendLog(std::string("JSON parse error: ") + e.what());
        appendLog("Body content: " + b);
    }

    initializeLines(extensions);

}

