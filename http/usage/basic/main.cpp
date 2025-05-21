#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#define PORT 8080

std::string build_response(const std::string& method, const std::string& body) {
    std::stringstream response;

    if (method == "GET") {
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/plain\r\n\r\n"
                 << "Hello from GET!\n";
    } else if (method == "POST") {
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: application/json\r\n\r\n"
                 << "{ \"message\": \"Received POST\", \"body\": \"" << body << "\" }\n";
    } else {
        response << "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
    }

    return response.str();
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address {};
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[4096] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    std::cout << "server running on http://localhost:" << PORT << "\n";

    while (true) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        read(new_socket, buffer, sizeof(buffer));

        std::string request(buffer);
        std::string method;
        std::string body;

        if (request.rfind("GET", 0) == 0) {
            method = "GET";
        } else if (request.rfind("POST", 0) == 0) {
            method = "POST";
            size_t pos = request.find("\r\n\r\n");
            if (pos != std::string::npos) {
                body = request.substr(pos + 4);
            }
        }

        std::string response = build_response(method, body);
        send(new_socket, response.c_str(), response.size(), 0);
        close(new_socket);
    }

    return 0;
}

