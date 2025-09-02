#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <openssl/ssl.h>
#include <openssl/sha.h>
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
                if (!write(client_socket))
                {
                    appendLog("SSL_write timeout");
                    goto cleanup_error;
                }
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


std::string readWebSocketMessage(int client_socket) 
{
    // 8-bit integer
    uint8_t header[2];
    int rc = recv(client_socket, header, 2, 0);
    if (rc != 2) return "";

    // 1-bit - FIN
    // 2-4 - RSV1-3
    // 5-8 - OPCODE
    bool fin = header[0] & 0x80;
    uint8_t opcode = header[0] & 0x0F;

    // 1-bit - MASK
    // 2-8 - PAYLOAD LEN
    // 126 means length is in next 2 bytes, 127 - 8 bytes
    bool mask = header[1] & 0x80;
    uint64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) {
        uint8_t ext[2]; 
        recv(client_socket, ext, 2, 0);
        payload_len = (ext[0] << 8) | ext[1];
    } 
    else if (payload_len == 127) {
        uint8_t ext[8]; 
        recv(client_socket, ext, 8, 0);
        payload_len = 0;
        for (int i=0; i<8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    // читаем mask (если есть)
    std::vector<uint8_t> masking_key(4);
    if (mask) {
        recv(client_socket, masking_key.data(), 4, 0);
    }

    // читаем полезные данные
    std::vector<uint8_t> payload(payload_len);
    size_t received = 0;
    while (received < payload_len) {
        int r = recv(client_socket, payload.data() + received, payload_len - received, 0);
        if (r <= 0) break;
        received += r;
    }

    // размаскируем (XOR)
    if (mask) {
        for (size_t i=0; i<payload_len; i++) {
            payload[i] ^= masking_key[i % 4];
        }
    }

    return std::string(payload.begin(), payload.end());
}

// создаём Sec-WebSocket-Accept
std::string generateWebSocketAccept(const std::string& key) {
    std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + guid;

    unsigned char hash[20]; // SHA1 всегда 20 байт
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), hash);

    // Base64 encode
    static const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    for (int i = 0; i < 20; i++) {
        val = (val << 8) + hash[i];
        valb += 8;
        while (valb >= 0) {
            result.push_back(b64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(b64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');

    return result;
}

// запуск сервера
void startWebSocketServer(int port) 
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_sock, 10);

    appendLog("WebSocket server listening on port " + std::to_string(port));

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) { 
            if (errno != EAGAIN && errno != EWOULDBLOCK) appendLog("accept error"); 
            continue; 
        }

        // читаем заголовки
        char buf[4096]; std::string headers;
        while (headers.find("\r\n\r\n") == std::string::npos) {
            int rc = recv(client_sock, buf, sizeof(buf), 0);
            if (rc <= 0) break;
            headers.append(buf, rc);
        }

        /*size_t key_pos = headers.find("Sec-WebSocket-Key:");
        if (key_pos == std::string::npos) { 
            close(client_sock); 
            continue; 
        }*/
        /*size_t key_end = headers.find("\r\n", key_pos);
        std::string key = headers.substr(key_pos + 18, key_end - (key_pos + 18));
        key.erase(0, key.find_first_not_of(" \t"));*/

        std::string accept_key = generateWebSocketAccept("key");
        std::ostringstream response;
        response << "HTTP/1.1 101 Switching Protocols\r\n"
                 << "Upgrade: websocket\r\n"
                 << "Connection: Upgrade\r\n"
                 << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";

        send(client_sock, response.str().data(), response.str().size(), 0);
        appendLog("WebSocket handshake completed with client");

        // цикл приёма сообщений
        while (true) {
            std::string msg = readWebSocketMessage(client_sock);
            if (msg.empty()) break;
            appendLog("Received WS message: " + msg);
        }

        close(client_sock);
    }
}

//<---------------------------------------------------------------->
//SSL Websocket server down below, cant quite figure it out for now

/*
//generating key for websocket
std::string generateWebSocketKey() 
{
    unsigned char rand_bytes[16];
    RAND_bytes(rand_bytes, sizeof(rand_bytes));
    std::ostringstream oss;
    for (auto b : rand_bytes) oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    return oss.str();
}



std::string readWebSocketMessage(SSL* ssl, int client_socket) 
{
    //8-bit integer
    uint8_t header[2];
    int rc = SSL_read(ssl, header, 2);
    if (rc != 2) return "";

    //1-bit - FIN
    //2-4 - RCV1
    //5-8 - OPCODE
    bool fin = header[0] & 0x80;
    uint8_t opcode = header[0] & 0x0F;

    //1-bit - MASK
    //2-8 - PAYLOAD LEN
    //126 means length is in next 2 bytes, 127 - 8 bytes
    bool mask = header[1] & 0x80;
    uint64_t payload_len = header[1] & 0x7F;

    if (payload_len == 126) 
    {
        uint8_t ext[2]; 
        SSL_read(ssl, ext, 2);
        payload_len = (ext[0] << 8) | ext[1];
    } 
    else if (payload_len == 127) 
    {
        uint8_t ext[8]; 
        SSL_read(ssl, ext, 8);
        payload_len = 0;
        for (int i=0; i<8; i++) 
        {
            payload_len = (payload_len << 8) | ext[i];
        }
    }

    //probably not needed, but i copied that anyways
    //code down below gets the mask
    std::vector<uint8_t> masking_key(4);
    if (mask) 
    {
        SSL_read(ssl, masking_key.data(), 4);
    }

    std::vector<uint8_t> payload(payload_len);
    size_t received = 0;

    //getting the useful payload data
    while (received < payload_len) 
    {
        int r = SSL_read(ssl, payload.data() + received, payload_len - received);
        if (r <= 0) 
        {
            break;
        }
        received += r;
    }

    //de-masking data if it's masked, just xor every byte with mask
    if (mask) 
    {
        for (size_t i=0; i<payload_len; i++) 
        {
            payload[i] ^= masking_key[i%4];
        }
    }

    return std::string(payload.begin(), payload.end());
}

std::string generateWebSocketAccept(const std::string& key) {
    std::string guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + guid;

    unsigned char hash[20]; // SHA1 всегда 20 байт
    SHA1(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.size(), hash);

    // Base64 encode
    static const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    int val = 0, valb = -6;
    for (int i = 0; i < 20; i++) {
        val = (val << 8) + hash[i];
        valb += 8;
        while (valb >= 0) {
            result.push_back(b64chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) result.push_back(b64chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (result.size() % 4) result.push_back('=');

    return result;
}

void startWebSocketServer(int port) 
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_sock, 10);

    appendLog("WebSocket server listening on port " + std::to_string(port));

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) { if (errno != EAGAIN && errno != EWOULDBLOCK) appendLog("accept error"); continue; }

        SSL* ssl = SSL_new(server_ctx);
        SSL_set_fd(ssl, client_sock);
        SSL_set_accept_state(ssl);

        // Handshake
        char buf[4096]; std::string headers;
        while (headers.find("\r\n\r\n") == std::string::npos) {
            int rc = SSL_read(ssl, buf, sizeof(buf));
            if (rc <= 0) break;
            headers.append(buf, rc);
        }

        size_t key_pos = headers.find("Sec-WebSocket-Key:");
        if (key_pos == std::string::npos) { SSL_shutdown(ssl); SSL_free(ssl); close(client_sock); continue; }
        size_t key_end = headers.find("\r\n", key_pos);
        std::string key = headers.substr(key_pos + 18, key_end - (key_pos + 18));
        key.erase(0, key.find_first_not_of(" \t"));

        std::string accept_key = generateWebSocketAccept(key);
        std::ostringstream response;
        response << "HTTP/1.1 101 Switching Protocols\r\n"
                 << "Upgrade: websocket\r\n"
                 << "Connection: Upgrade\r\n"
                 << "Sec-WebSocket-Accept: " << accept_key << "\r\n\r\n";

        SSL_write(ssl, response.str().data(), response.str().size());
        appendLog("WebSocket handshake completed with client");

        // Цикл приема сообщений
        while (true) {
            std::string msg = readWebSocketMessage(ssl, client_sock);
            if (msg.empty()) break;
            appendLog("Received WS message: " + msg);
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_sock);
    }
}
*/

//websocket client down below, not needed for now
/*
bool sendWebSocketHandshake(SSL* ssl)
{
    std::string ws_key = generateWebSocketKey();

    char req[1024];
    snprintf(req, sizeof(req),
    "GET https://app.mango-office.ru HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: %s\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n",
    hostname, ws_key.c_str());

    size_t sent = 0;
    int total = strlen(req);
    while (sent < total) 
    {
        int rc = SSL_write(ssl, req + sent, static_cast<int>(total - sent));
        if (rc > 0) 
        {
            sent += rc;
            continue;
        }

        int err = SSL_get_error(ssl, rc);
        if (err == SSL_ERROR_WANT_WRITE) 
        {
            if (!write(SSL_get_fd(ssl))) 
            {
                appendLog("SSL_write WANT_WRITE timeout");
                return false;
            }
        } 
        else if (err == SSL_ERROR_WANT_READ) 
        {
            if (!read(SSL_get_fd(ssl))) 
            {
                appendLog("SSL_write WANT_READ timeout");
                return false;
            }
        } 
        else if (err == SSL_ERROR_ZERO_RETURN) 
        {
            appendLog("SSL connection closed by peer during write");
            return false;
        } 
        else if (err == SSL_ERROR_SYSCALL) 
        {
            appendLog("SSL_write syscall error");
            return false;
        } 
        else // SSL_ERROR_SSL or unknown
        {
            appendLog("SSL_write error: " + std::string(ERR_error_string(ERR_get_error(), nullptr)));
            return false;
        }
    }
    return true;
    
}

void startWebSocketClient() 
{
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, "443", &hints, &res);
    if (status != 0) {
        std::string error = "getaddrinfo error in ws connecton: " + std::string(gai_strerror(status));
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
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
        return;
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
        return;
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
        return;
    }

    if (!write(client_socket)) 
    {
        std::string error = "client socket is not ready after connect()";
        appendLog(error);
        #ifdef DEBUG
        std::cerr << error << std::endl;
        #endif
        close(client_socket);
        freeaddrinfo(res);
        return;
    }

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
        return;
    }

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
        return;
    }

    SSL_set_tlsext_host_name(ssl, host);
    SSL_set_fd(ssl, client_socket);
    SSL_set_connect_state(ssl);

    while (true) 
    {
        int rc = SSL_connect(ssl);
        if (rc == 1) 
        {
            break;
        }
        int err = SSL_get_error(ssl, rc);
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

    cleanup_error:
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    freeaddrinfo(res);

    std::string ws_key;
    if (!sendWebSocketHandshake(ssl)) 
    {
        appendLog("Handshake failed");
        return;
    }

    char buf[4096]; std::string headers;
    while (headers.find("\r\n\r\n") == std::string::npos)
    {
        int rc = SSL_read(ssl, buf, sizeof(buf));
        if (rc <= 0) 
        {
            break;
        }
        headers.append(buf, rc);
    }

    appendLog("WebSocket handshake successful");

    //recieving cycle
    while (true)
    {
        std::string msg = readWebSocketMessage(ssl, client_socket);
        if (msg.empty()) 
        {
            break;
        }
        appendLog("Received WS message: " + msg);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    freeaddrinfo(res);
}

*/