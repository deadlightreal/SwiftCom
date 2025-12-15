#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include "net.hpp"

in_addr utils::net::get_public_ip() {
    const char* host = "api.ipify.org";
    const char* port = "80";

    addrinfo hints{};
    addrinfo* res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        perror("socket");
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sock);
        freeaddrinfo(res);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);

    std::string request = 
        "GET / HTTP/1.1\r\n"
        "Host: api.ipify.org\r\n"
        "Connection: close\r\n\r\n";

    send(sock, request.c_str(), request.size(), 0);

    char buffer[4096];
    std::string response;
    ssize_t bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }

    close(sock);

    size_t pos = response.find("\r\n\r\n");
    if (pos != std::string::npos) {
        std::string body = response.substr(pos + 4);
        std::cout << "Public IP: \"" << body << '\"' << std::endl;
        struct in_addr public_ip;

        int result = inet_pton(AF_INET, body.c_str(), &public_ip);
        if (result == 1) {
            return public_ip;
        } else {
            perror("inet_pton");
            exit(EXIT_FAILURE);
        }
    } else {
        std::cerr << "Failed to parse response:\n" << response << std::endl;
        exit(EXIT_FAILURE);
    }
}
