#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

int main() {
    // tcp socket using ipv4 (AF_INET)
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(8080); // port number from host byte order to network byte order
    server.sin_addr.s_addr = INADDR_ANY; // listen to any ip address

    bind(sock, reinterpret_cast<struct sockaddr *>(&server), sizeof(server));
    listen(sock, 5); // to passive mode + max conn set to 5

    std::cout << "listening tcp on 8080...\n";
    const int client = accept(sock, nullptr, nullptr);
    char buffer[1024];
    read(client, buffer, sizeof(buffer)); // read data from the connected client
                                          // upto 1024 into buffer

    std::cout << "received: " << buffer << std::endl;
    close(client);
    close(sock);
}
