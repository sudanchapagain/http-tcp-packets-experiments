#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <zlib.h>

#define BUFFER_SIZE 1024

typedef enum {
    CONTENT_TYPE_TEXT,
    CONTENT_TYPE_FILE
} ContentType;

typedef enum {
    ENCODING_TYPE_GZIP
} EncodingType;

int gzip_compress(const unsigned char *data, size_t data_len, unsigned char *compressed, size_t *compressed_len) {
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return -1;
    }

    stream.avail_in = data_len;
    stream.next_in = (unsigned char *)data;
    stream.avail_out = *compressed_len;
    stream.next_out = compressed;

    if (deflate(&stream, Z_FINISH) != Z_STREAM_END) {
        deflateEnd(&stream);
        return -1;
    }

    *compressed_len = stream.total_out;
    deflateEnd(&stream);

    return 0;
}

void parse_headers(const char *request, char *user_agent) {
    const char *header_start = strstr(request, "User-Agent: ");
    if (header_start) {
        header_start += strlen("User-Agent: ");
        const char *header_end = strstr(header_start, "\r\n");
        if (header_end) {
            size_t length = header_end - header_start;
            strncpy(user_agent, header_start, length);
            user_agent[length] = '\0';
        }
    }
}

void handle_connection(int client_sock) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received < 0) {
        perror("recv failed");
        close(client_sock);
        return;
    }

    buffer[bytes_received] = '\0';
    printf("Received request:\n%s\n", buffer);

    char *method = strtok(buffer, " ");
    char *path = strtok(NULL, " ");
    char *version = strtok(NULL, "\r\n");

    if (method == NULL || path == NULL || version == NULL) {
        const char *response_body = "400 Bad Request";
        const char *status_line = "HTTP/1.1 400 Bad Request\r\n";
        const char *content_type_str = "Content-Type: text/plain\r\n";
        int response_len = snprintf(buffer, sizeof(buffer),
                                    "%s%sContent-Length: %zu\r\n\r\n%s",
                                    status_line, content_type_str, strlen(response_body), response_body);
        send(client_sock, buffer, response_len, 0);
        close(client_sock);
        return;
    }

    const char *status_line;
    const char *content_type_str = "Content-Type: text/plain\r\n";
    char response_body[BUFFER_SIZE];
    size_t content_length;

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0) {
            snprintf(response_body, sizeof(response_body), "Hello, world!");
            status_line = "HTTP/1.1 200 OK\r\n";
        } else if (strncmp(path, "/echo/", 6) == 0) {
            snprintf(response_body, sizeof(response_body), "%s", path + 6);
            status_line = "HTTP/1.1 200 OK\r\n";
        } else if (strcmp(path, "/user-agent") == 0) {
            char user_agent[BUFFER_SIZE];
            parse_headers(buffer, user_agent);
            snprintf(response_body, sizeof(response_body), "%s", user_agent);
            status_line = "HTTP/1.1 200 OK\r\n";
        } else {
            const char *response_body_404 = "404 Not Found";
            content_length = strlen(response_body_404);
            strcpy(response_body, response_body_404);
            status_line = "HTTP/1.1 404 Not Found\r\n";
        }
    } else {
        const char *response_body_400 = "400 Bad Request";
        content_length = strlen(response_body_400);
        strcpy(response_body, response_body_400);
        status_line = "HTTP/1.1 400 Bad Request\r\n";
        content_type_str = "Content-Type: text/plain\r\n";
    }

    content_length = strlen(response_body);
    int headers_len = snprintf(buffer, sizeof(buffer),
                               "%s%sContent-Length: %zu\r\n\r\n",
                               status_line, content_type_str, content_length);
    snprintf(buffer + headers_len, sizeof(buffer) - headers_len, "%s", response_body);
    int response_len = headers_len + content_length;

    send(client_sock, buffer, response_len, 0);
    close(client_sock);
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    printf("Logs from your program will appear here!\n");

    int server_fd, client_fd;
    socklen_t client_addr_len;
    struct sockaddr_in serv_addr, client_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s\n", strerror(errno));
        return 1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s\n", strerror(errno));
        return 1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(4221);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s\n", strerror(errno));
        return 1;
    }

    if (listen(server_fd, 5) != 0) {
        printf("Listen failed: %s\n", strerror(errno));
        return 1;
    }

    printf("Waiting for a client to connect...\n");
    client_addr_len = sizeof(client_addr);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s\n", strerror(errno));
            continue;
        }

        printf("Client connected\n");
        handle_connection(client_fd);
    }

    close(server_fd);
    return 0;
}
