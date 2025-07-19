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

#include "settings.h"

static SSL_CTX* client_ctx;
static SSL_CTX* server_ctx;

std::string hostname = "https://app.mango-office.ru/vpbx";

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

int sendMessage()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket <0)
    {
        perror("clientSocket");
        return 1;
    }
    fcntl(clientSocket , F_SETFL, O_NONBLOCK);//nonblocking port

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(443);
    inet_pton(AF_INET, "81.88.85.67", &server_addr.sin_addr);

    int con = connect(clientSocket, (sockaddr*)&server_addr,sizeof(server_addr));
    if(con<0 && errno != EINPROGRESS)
    {
        perror("clientConnect");
        close(clientSocket);
        return 1;
    }
    return 1;
}

void getLines()
{

}