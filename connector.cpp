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
#include <variant>
#include <optional>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstring>
#include <functional>
#include <atomic>

#include "settings.h"


using json = nlohmann::json;

std::atomic<bool> server_running = true;
std::atomic<bool> pipe_thread_running{true};
std::thread pipe_thread;
int pipe_fd[2];

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


// читаем тело запроса полностью
std::string readHttpBody(int client_socket, size_t content_length) {
    std::string body;
    body.resize(content_length);
    size_t received = 0;

    while (received < content_length) {
        int r = recv(client_socket, body.data() + received, content_length - received, 0);
        if (r <= 0) break;
        received += r;
    }
    return body;
}

//decoding body, cause it is url-encoded
std::string urlDecode(const std::string &str) 
{
    std::ostringstream decoded;
    for (size_t i = 0; i < str.size(); ++i) 
    {
        if (str[i] == '%' && i + 2 < str.size()) 
        {
            int val;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> val) 
            {
                decoded << static_cast<char>(val);
                i += 2;
            }
        } 
        else if (str[i] == '+') 
        {
            decoded << ' ';
        } 
        else 
        {
            decoded << str[i];
        }
    }
    return decoded.str();
}

//data structures that store /call events

struct AuthEvent 
{
    std::string entry_id;
    int product_id;
    int user_id;
    int64_t timestamp;
    std::string recording_id;
};

struct CallStateEvent 
{
    std::string entry_id;
    std::string call_id;
    int64_t timestamp;
    int seq;
    std::string call_state; //Appeared, Connected, Disconnected
    std::string location;

    std::string from_extension;
    std::string from_number;
    std::string from_line_number;

    std::string to_number;

    std::optional<int> disconnect_reason; //только для Disconnected
    std::optional<int> dct_type; //Appeared, Connected, Disconnected
    std::string sip_call_id;
};

struct RecordingEvent {
    std::string recording_id;
    std::string recording_state; // Started, Completed, Stopped
    int seq;
    std::string entry_id;
    std::string call_id;
    std::string extension;
    int64_t timestamp;
    std::string recipient;

    std::optional<int> completion_code;
};

struct CallSummary 
{
    std::string entry_id;
    int call_direction; //1 - incoming, 2 - outgoing
    std::string from_extension;
    std::string from_number;
    std::string to_number;
    std::string line_number;

    int64_t create_time;
    int64_t forward_time;
    int64_t talk_time;
    int64_t end_time;

    int entry_result;
    int disconnect_reason;
    std::string sip_call_id;
};


//custom observable map for in-real-time calltracking, emits signals when edited
template<typename K, typename V>
class ObservableMap 
{
    std::map<K, V> data;

public:
    using Listener = std::function<void()>;

private:
    std::vector<Listener> listeners;

    void notify() 
    {
        for (auto &fn : listeners) fn();
    }

public:
    //subscribing for notifying
    void subscribe(Listener fn) 
    {

        listeners.push_back(fn);
    }
    
    //standart methods for map
    void insertOrUpdate(const K& key, const V& value) 
    {
        data[key] = value;
        notify();
    }

    //erase for every possible way to use it
    void erase(const K& key)
    {
        auto it = data.find(key);
        if (it != data.end()) 
        {
            data.erase(it);
            notify();
        }
    }
    void erase(typename std::map<K, V>::iterator pos) 
    {
        data.erase(pos);
        notify();
    }
    void erase(typename std::map<K, V>::const_iterator pos) 
    {
        data.erase(pos);
        notify();
    }

    void erase(typename std::map<K, V>::iterator first, typename std::map<K, V>::iterator last) 
    {
        if (first != last) 
        {
            data.erase(first, last);
            notify();
        }
    }

    //access like in std::map
    V& operator[](const K& key) 
    {
        notify(); 
        return data[key];
    }

    V& at(const K& key) 
    {
    return data.at(key);
    }
    const V& at(const K& key) const 
    {
        return data.at(key);
    }

    auto clear() { return data.clear(); }

    std::size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }

    bool operator==(const ObservableMap& other) const 
    {
        return data == other.data;
    }
    bool operator!=(const ObservableMap& other) const 
    {
        return data != other.data;
    }

    auto find(const K& key) { return data.find(key); }
    auto find(const K& key) const { return data.find(key); }

    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
};

//map for storing active calls
ObservableMap<std::string, CallStateEvent> activeCalls;

//function that parses calls (only CallStateEvent) to map and controls call state flow
void storeCallState(const CallStateEvent& ev) 
{
    std::cout << "DEBUG: call_state = '" << ev.call_state << "'" << std::endl;
    std::cout << "DEBUG: call_id = '" << ev.call_id << "'" << std::endl;

    if (ev.call_state == "Appeared") 
    {
        activeCalls.insertOrUpdate(ev.call_id,ev);
    }
    else if (ev.call_state == "Connected") 
    {
        auto it = activeCalls.find(ev.call_id);
        if (it != activeCalls.end()) 
        {
            activeCalls.insertOrUpdate(ev.call_id,ev);
        }
        //in some cases second call state can come earlier than first
        //if call not found i just add it in for now, gotta rework that later
        else 
        {
            activeCalls.insertOrUpdate(ev.call_id,ev);
        }
    }
    else if (ev.call_state == "Disconnected") 
    {
        auto it = activeCalls.find(ev.call_id);
        //here i will search for call_summary and add it to posgresql
        if (it != activeCalls.end()) 
        {
            #ifdef DEBUG
            std::cout << "Звонок завершён: " << ev.call_id 
                      << " seq=" << ev.seq 
                      << " timestamp=" << ev.timestamp << "\n";
            #endif

            activeCalls.erase(it);
        }
    }
}

using CallEvent = std::variant<AuthEvent, CallStateEvent, RecordingEvent, CallSummary>;
static CallEvent call_state;

//this map stores every /call event before it being redirected, not needed in current state
//std::map<std::string, CallEvent> events;

//this function distributes events to data structures
CallEvent parseEvent(const std::string& jsonStr) {
    auto j = nlohmann::json::parse(jsonStr);

    if (j.contains("call_state")) 
    {
        CallStateEvent ev;
        ev.entry_id = j.value("entry_id", "");
        ev.call_id = j.value("call_id", "");
        ev.timestamp = j.value("timestamp", 0);
        ev.seq = j.value("seq", 0);
        ev.call_state = j.value("call_state", "");
        ev.location = j.value("location", "");
        if (j.contains("from")) 
        {
            ev.from_extension = j["from"].value("extension", "");
            ev.from_number = j["from"].value("number", "");
            ev.from_line_number = j["from"].value("line_number", "");
        }
        if (j.contains("to")) 
        {
            ev.to_number = j["to"].value("number", "");
        }
        if (j.contains("disconnect_reason"))
            ev.disconnect_reason = j["disconnect_reason"].get<int>();
        if (j.contains("dct"))
            ev.dct_type = j["dct"].value("type", 0);
        ev.sip_call_id = j.value("sip_call_id", "");
        return ev;
    }
    else if (j.contains("recording_state")) 
    {
        RecordingEvent ev;
        ev.recording_id = j.value("recording_id", "");
        ev.recording_state = j.value("recording_state", "");
        ev.seq = j.value("seq", 0);
        ev.entry_id = j.value("entry_id", "");
        ev.call_id = j.value("call_id", "");
        ev.extension = j.value("extension", "");
        ev.timestamp = j.value("timestamp", 0);
        ev.recipient = j.value("recipient", "");
        if (j.contains("completion_code"))
            ev.completion_code = j["completion_code"].get<int>();
        return ev;
    }
    else if (j.contains("call_direction")) 
    {
        CallSummary ev;
        ev.entry_id = j.value("entry_id", "");
        ev.call_direction = j.value("call_direction", 0);
        if (j.contains("from")) 
        {
            ev.from_extension = j["from"].value("extension", "");
            ev.from_number = j["from"].value("number", "");
        }
        if (j.contains("to")) 
        {
            ev.to_number = j["to"].value("number", "");
        }
        ev.line_number = j.value("line_number", "");
        ev.create_time = j.value("create_time", 0);
        ev.forward_time = j.value("forward_time", 0);
        ev.talk_time = j.value("talk_time", 0);
        ev.end_time = j.value("end_time", 0);
        ev.entry_result = j.value("entry_result", 0);
        ev.disconnect_reason = j.value("disconnect_reason", 0);
        ev.sip_call_id = j.value("sip_call_id", "");
        return ev;
    }
    else if (j.contains("product_id")) 
    {
        AuthEvent ev;
        ev.entry_id = j.value("entry_id", "");
        ev.product_id = j.value("product_id", 0);
        ev.user_id = j.value("user_id", 0);
        ev.timestamp = j.value("timestamp", 0);
        ev.recording_id = j.value("recording_id", "");
        return ev;
    }
    else if (j.contains("key")) 
    {
        return {};
    }

    throw std::runtime_error("Unknown event type");
}

const char* _PID_FILE = "/tmp/app_release.pid";

bool isServerRunning() 
{
    //checking if pid file exists
    std::ifstream pidFile(_PID_FILE);
    if (!pidFile.is_open()) return false;

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    //pid == 0 means that server is already running
    if (kill(pid, 0) == 0) 
    {
        return true;
    } 
    return false;
}

//saving new pid file in case of it not existing
void savePid() 
{
    std::ofstream pidFile(_PID_FILE);
    pidFile << getpid();
    pidFile.close();
}

//deleting pid file
void removePid() 
{
    std::filesystem::remove(_PID_FILE);
}

//turns out pipes cant work with strings and optionals, this serialized data for it to go throuhg pipe correctly
//CallStateEvent hat strings and optionals, while pipe can only send chars safely
//this scruct makes sure data is serialized properly
struct SerializableCallStateEvent 
{
    char type; //'U' - update, 'R' - remove
    char call_id[65];
    char entry_id[65];
    char call_state[20];
    char from_extension[20];
    char from_number[20];
    char from_line_number[20];
    char to_number[20];
    char sip_call_id[100];
    int64_t timestamp;
    int seq;
    int disconnect_reason; //-1 if empty
    int dct_type; //-1 if empty
    
    //constructor that prepares memory with memset
    SerializableCallStateEvent() : type(' '), timestamp(0), seq(0), 
                                  disconnect_reason(-1), dct_type(-1) 
    {
        memset(call_id, 0, sizeof(call_id));
        memset(entry_id, 0, sizeof(entry_id));
        memset(call_state, 0, sizeof(call_state));
        memset(from_extension, 0, sizeof(from_extension));
        memset(from_number, 0, sizeof(from_number));
        memset(from_line_number, 0, sizeof(from_line_number));
        memset(to_number, 0, sizeof(to_number));
        memset(sip_call_id, 0, sizeof(sip_call_id));
    }
    
    //converter from
    static SerializableCallStateEvent fromCallStateEvent(const CallStateEvent& ev, char msg_type = 'U') 
    {
        SerializableCallStateEvent serial;
        serial.type = msg_type;
        
        //copying strings with length check
        copyString(serial.call_id, ev.call_id, sizeof(serial.call_id));
        copyString(serial.entry_id, ev.entry_id, sizeof(serial.entry_id));
        copyString(serial.call_state, ev.call_state, sizeof(serial.call_state));
        copyString(serial.from_extension, ev.from_extension, sizeof(serial.from_extension));
        copyString(serial.from_number, ev.from_number, sizeof(serial.from_number));
        copyString(serial.from_line_number, ev.from_line_number, sizeof(serial.from_line_number));
        copyString(serial.to_number, ev.to_number, sizeof(serial.to_number));
        copyString(serial.sip_call_id, ev.sip_call_id, sizeof(serial.sip_call_id));
        
        serial.timestamp = ev.timestamp;
        serial.seq = ev.seq;
        
        //optionals
        serial.disconnect_reason = ev.disconnect_reason.value_or(-1);
        serial.dct_type = ev.dct_type.value_or(-1);
        
        return serial;
    }
    
    //converting to
    CallStateEvent toCallStateEvent() const 
    {
        CallStateEvent ev;
        ev.call_id = call_id;
        ev.entry_id = entry_id;
        ev.call_state = call_state;
        ev.from_extension = from_extension;
        ev.from_number = from_number;
        ev.from_line_number = from_line_number;
        ev.to_number = to_number;
        ev.sip_call_id = sip_call_id;
        ev.timestamp = timestamp;
        ev.seq = seq;
        
        if (disconnect_reason != -1) 
        {
            ev.disconnect_reason = disconnect_reason;
        }
        if (dct_type != -1) 
        {
            ev.dct_type = dct_type;
        }
        
        return ev;
    }
    
private:
    //this always sets last byte to null terminator
    static void copyString(char* dest, const std::string& src, size_t dest_size) 
    {
        strncpy(dest, src.c_str(), dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
};

//async server
void startHttpServer(int port, int write_pipe_fd) 
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    //htons = host-to-network-short
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_sock, 10);

    appendLog("HTTP server listening on port " + std::to_string(port));

    std::vector<int> clients;

    //might not be neccecary to put it here
    struct PipeMessage 
    {
        char type; // 'U' - update, 'R' - remove, 'C' - clear
        char call_id[65]; //+1 for null term
        CallStateEvent event;
        
        // Конструктор для инициализации
        PipeMessage() : type(' ') 
        {
            memset(call_id, 0, sizeof(call_id));
        }
    };

    //main loop
    while (server_running) 
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock >= 0) 
        {
            fcntl(client_sock, F_SETFL, O_NONBLOCK);
            clients.push_back(client_sock);
            appendLog("Accepted new client: " + std::to_string(client_sock));
        }
        
        //creating and zeroing fd set
        fd_set read_fds;
        FD_ZERO(&read_fds);

        //adding server socked to fd set
        FD_SET(listen_sock, &read_fds);
        int max_fd = listen_sock;

        //adding client sockets
        for (int c : clients) 
        {
            FD_SET(c, &read_fds);
            if (c > max_fd) 
            {
                max_fd = c;
            }
        }

        timeval tv{0, 50000};
        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, &tv);

        if (ready > 0) 
        {
            std::vector<int> closed_clients;
            for (int c : clients) 
            {
                //if socket is ready to be read
                if (FD_ISSET(c, &read_fds)) 
                {
                    char buf[4096];
                    std::string headers;
                    int rc = recv(c, buf, sizeof(buf), 0);
                    //0 = client closed connection
                    //-1 = error
                    if (rc <= 0) 
                    {
                        closed_clients.push_back(c);
                        continue;
                    }
                    headers.append(buf, rc);

                    //parsing headers
                    size_t pos = headers.find("\r\n\r\n");
                    if (pos != std::string::npos) 
                    {
                        
                        std::string header_block = headers.substr(0, pos);
                        #ifdef DEBUG
                            //appendLog("Received headers from client " + std::to_string(c) + ":");
                            //appendLog(header_block);
                            //std::cout << "Received headers from client " + std::to_string(c) + ":" << header_block << std::endl;
                        #endif

                        size_t content_length = 0;
                        std::string lower_header = header_block;
                        std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(), ::tolower);
                        size_t cl_pos = lower_header.find("content-length:");
                        if (cl_pos != std::string::npos) 
                        {
                            size_t cl_end = lower_header.find("\r\n", cl_pos);
                            std::string cl_str = header_block.substr(cl_pos + 15, cl_end - (cl_pos + 15));
                            content_length = std::stoul(cl_str);
                        }
                        
                        //parsing body
                        size_t header_end = headers.find("\r\n\r\n");
                        if (header_end != std::string::npos) 
                        {

                            std::string body = headers.substr(header_end + 4);
                            //decoding body
                            std::string body_decoded = urlDecode(body);

                            appendLog("Received body from client " + std::to_string(c) + ":");
                            appendLog(body_decoded);

                            //recieving raw json from body
                            size_t pos = body_decoded.find("json=");
                            if (pos == std::string::npos)
                            {
                                std::cerr << "No JSON found during event recieving" << std::endl;
                            }
                            std::string jsonPart = body_decoded.substr(pos + 5); 

                            call_state = parseEvent(jsonPart);

                            if (std::holds_alternative<CallStateEvent>(call_state)) 
                            {
                                auto ev = std::get<CallStateEvent>(call_state);
                                appendLog("CallStateEvent: " + ev.call_state + " for call_id=" + ev.call_id);


                                //creating pipe message
                                PipeMessage msg;
                                msg.type = 'U';

                                //copying safely with null term at the end
                                if (ev.call_id.size() < sizeof(msg.call_id) - 1) 
                                {
                                    strncpy(msg.call_id, ev.call_id.c_str(), sizeof(msg.call_id) - 1);
                                    msg.call_id[sizeof(msg.call_id) - 1] = '\0';
                                } 
                                else 
                                {
                                    //cutting if too long
                                    strncpy(msg.call_id, ev.call_id.c_str(), sizeof(msg.call_id) - 1);
                                    msg.call_id[sizeof(msg.call_id) - 1] = '\0';
                                    appendLog("CallID is too long to go through pipe!!!");
                                }

                                SerializableCallStateEvent serial = SerializableCallStateEvent::fromCallStateEvent(ev, 'U');
                                #ifdef DEBUG
                                std::cout << "SENDING: call_id = '" << serial.call_id 
                                        << "', call_state = '" << serial.call_state << "'" << std::endl;
                                #endif

                                ssize_t bytes_written = write(write_pipe_fd, &serial, sizeof(serial));
                                if (bytes_written != sizeof(serial))
                                {
                                    appendLog("Failed to send pipe message: " + std::string(strerror(errno)));
                                }

                                //debugging active calls
                                #ifdef DEBUG
                                {
                                    if (activeCalls.empty()) 
                                    {
                                        std::cout << "Нет активных звонков\n";
                                    } 
                                    else 
                                    {
                                        for (auto& [id, call] : activeCalls) 
                                        {
                                            std::cout << "Активный звонок: " << id 
                                                    << " состояние: " << call.call_state << "\n";
                                        }
                                    }
                                
                                }
                                #endif
                            }
                            else if (std::holds_alternative<RecordingEvent>(call_state)) 
                            {
                                auto ev = std::get<RecordingEvent>(call_state);
                                appendLog("RecordingEvent: " + ev.recording_state + " for call_id=" + ev.call_id);
                            }
                            else if (std::holds_alternative<CallSummary>(call_state)) 
                            {
                                auto ev = std::get<CallSummary>(call_state);
                                appendLog("CallSummary: " + ev.entry_id + " direction=" + std::to_string(ev.call_direction));
                            }
                        }
                        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK";
                        send(c, response.data(), response.size(), 0);

                        closed_clients.push_back(c);
                    }
                }
            }

            for (int c : closed_clients) 
            {
                close(c);
                //move all closed clients in clients to end and erase them
                clients.erase(std::remove(clients.begin(), clients.end(), c), clients.end());
            }
        }
    }
    PipeMessage msg;
    msg.type = 'C';
    write(write_pipe_fd, &msg, sizeof(msg));
    
    close(write_pipe_fd);

    activeCalls.clear();
    appendLog("Server shut down succesfully.\n");
    std::cout << "Server shut down succesfully." << std::endl;
    
}

//method looks for installed terminals, made for a bit of versatility
bool isInstalled(const std::string &cmd) 
{
    std::string check = "which " + cmd + " > /dev/null 2>&1";
    int ret = system(check.c_str());
    return WIFEXITED(ret) && WEXITSTATUS(ret) == 0;
}

//runs terminal with a dedicated script, might be reused
bool tryRunTerminal(const std::string &terminal, const std::string &scriptPath) 
{
    std::string cmd;
    if (terminal == "gnome-terminal") 
    {
        cmd = terminal + " -- " + scriptPath;
    } 
    else if (terminal == "konsole") 
    {
        cmd = terminal + " -e " + scriptPath;
    } 
    else if (terminal == "xterm") 
    {
        cmd = terminal + " -e " + scriptPath;
    } 
    else if (terminal == "terminator") 
    {
        cmd = terminal + " -x " + scriptPath;
    } 
    else 
    {
        return false;
    }
    return system(cmd.c_str()) == 0;
}

void displayCalls() 
{
    
    //creating pipe to deliver data inside newly opened terminal
    const char* fifo = "/tmp/my_pipe";
    mkfifo(fifo, 0666);

    const char* scriptPath = "/tmp/show_table.sh";
    std::ofstream script(scriptPath);
    script << "#!/bin/bash\n"
           << "while true; do\n"
           << "    clear\n"
           << "    cat /tmp/my_pipe\n"
           << "    sleep 1\n"
           << "done\n";
    script.close();
    //groups and others cannot write
    chmod(scriptPath, 0755);

    std::vector<std::string> terminals = {"gnome-terminal", "konsole", "xterm", "terminator"};

    bool launched = false;
    for (const auto &term : terminals) 
    {
        if (!isInstalled(term)) continue;

        if (tryRunTerminal(term, scriptPath)) 
        {
            launched = true;
            break;
        } 
        else 
        {
            std::string error = "Не удалось запустить " + term + ". Переходим к следующему.\n";
            appendLog(error);
            #ifdef DEBUG
                std::cerr << "Не удалось запустить " << term << ". Переходим к следующему." << std::endl;
            #endif
        }
    }

    if (!launched) 
    {
        std::string error = "Не удалось запустить ни один терминал.\n";
        appendLog(error);
        #ifdef DEBUG
            std::cerr << "Не удалось запустить ни один терминал." << std::endl;
        #endif
        return;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    static std::ofstream out(fifo);
    if (!out.is_open()) 
    {
        std::string error = "Не удалось открыть FIFO на запись.\n";
        appendLog(error);
        #ifdef DEBUG
        {
            std::cerr << "Не удалось открыть FIFO на запись" << std::endl;   
        }
        #endif
        return;
    }
    static std::string buf;
    //subbing to map changes, lambda refreshes the pipe
    activeCalls.subscribe([&]() 
    {
        std::cout << "event" << std::endl;
        std::string ret;
        for(auto const& [key,val] : activeCalls)
        {
            std::cout << val.call_state << std::endl;
            CallStateEvent call = val;
            buf = val.from_number + " " + val.call_state + " " + val.location + "\n";
            ret.append(buf);
            

        };
        //this code clears terminal
        out << "\033[2J\033[H";
        out << ret << std::flush;
    });
    
    /*
        ev.entry_id = j.value("entry_id", "");
        ev.call_id = j.value("call_id", "");
        ev.timestamp = j.value("timestamp", 0);
        ev.seq = j.value("seq", 0);
        ev.call_state = j.value("call_state", "");
        ev.location = j.value("location", "");
        if (j.contains("from")) 
        {
            ev.from_extension = j["from"].value("extension", "");
            ev.from_number = j["from"].value("number", "");
            ev.from_line_number = j["from"].value("line_number", "");
        }
        if (j.contains("to")) 
        {
            ev.to_number = j["to"].value("number", "");
        }
        if (j.contains("disconnect_reason"))
            ev.disconnect_reason = j["disconnect_reason"].get<int>();
        if (j.contains("dct"))
            ev.dct_type = j["dct"].value("type", 0);
        ev.sip_call_id = j.value("sip_call_id", "");
        return ev; */


}

//struct for transfering data from server process to parent
struct PipeMessage 
{
    char type; //'U' - update, 'R' - remove, 'C' - clear
    char call_id[65]; //+1 for null term
    CallStateEvent event;
    
    //consctructor is needed to initialise "clear" pipe message before filling it with data
    PipeMessage() : type(' ') 
    {
        memset(call_id, 0, sizeof(call_id));
    }
};


void processPipeMessage(const PipeMessage& msg) 
{
    #ifdef DEBUG
    if (msg.call_id[0] == '\0') 
    {
        std::cout << "ERROR: Empty call_id in pipe message" << std::endl;
        return;
    }
    
    std::string call_id(msg.call_id);
    
    if (msg.event.call_state.empty()) {
        std::cout << "ERROR: Empty call_state for call_id: " << call_id << std::endl;
        return;
    }
    
    std::cout << "PROCESSING: call_id = '" << call_id 
              << "', call_state = '" << msg.event.call_state << "'" << std::endl;
    #endif
    
    switch (msg.type) 
    {
        case 'U': //update
            storeCallState(msg.event);
            break;
            
        case 'R': //remove
            activeCalls.erase(call_id);
            break;
            
        case 'C': //clear all
            activeCalls.clear();
            break;
            
        default:
            std::cout << "Unknown message type: '" << msg.type << "'" << std::endl;
    }
}

void handlePipeMessagesThread() 
{
    SerializableCallStateEvent serial;
    
    while (pipe_thread_running) {
        memset(&serial, 0, sizeof(serial));
        
        ssize_t bytes_read = read(pipe_fd[0], &serial, sizeof(serial));
        
        if (bytes_read == sizeof(serial)) {
            std::cout << "RECEIVED: type = '" << serial.type 
                      << "', call_id = '" << serial.call_id 
                      << "', call_state = '" << serial.call_state << "'" << std::endl;
            
            if (serial.type == 'U' && serial.call_id[0] != '\0') {
                CallStateEvent ev = serial.toCallStateEvent();
                storeCallState(ev);
            }
        }else {
            appendLog("Incomplete pipe message: " + std::to_string(bytes_read) + "/" + 
                     std::to_string(sizeof(serial)) + " bytes");
        }
    }
}

void stopPipeThread() 
{
    pipe_thread_running = false;
    if (pipe_thread.joinable()) {
        pipe_thread.join();
    }
}


int startServer()
{
    if (isServerRunning()) 
    {
        std::cout << "Server already running. Exiting.\n";
        return 0;
    }

    //pipe for transfering call data from httpServer, has to be initialized before fork
    if (pipe(pipe_fd) == -1) 
    {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) return 1;

    if (pid > 0) 
    {

        close(pipe_fd[1]); 
        std::cout << "Server launched (pid=" << pid << ")\n";
        
        pipe_thread_running = true;
        pipe_thread = std::thread(handlePipeMessagesThread);

        return 0;
    }

    

    close(pipe_fd[0]);

    setsid(); //disconnecting from terminal
    savePid();

    server_running = true;
    startHttpServer(8058,pipe_fd[1]);
    return 1;
}

int stopServer()
{
    std::cout << "Shutting down the server..." << std::endl; 

    stopPipeThread();

    std::ifstream pidFile(_PID_FILE);
    if (!pidFile.is_open()) 
    {
        std::cerr << "PID file not found. Server may not be running.\n";
        return 1;
    }

    pid_t pid;
    pidFile >> pid;
    pidFile.close();

    if (kill(pid, SIGTERM) == 0) 
    {
        std::cout << "Server stopped (pid=" << pid << ")\n";
        removePid();
        return 0;
    } 
    else 
    {
        perror("kill");
        return 1;
    }
}

