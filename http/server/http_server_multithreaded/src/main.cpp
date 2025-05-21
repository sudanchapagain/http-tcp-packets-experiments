#include <netinet/in.h>
#include <unistd.h>

#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;

    std::string to_string() const {
        std::ostringstream response;
        response << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        for (const auto& [k, v] : headers) {
            response << k << ": " << v << "\r\n";
        }
        response << "\r\n" << body;
        return response.str();
    }
};

HttpRequest parse_request(const std::string& raw) {
    std::istringstream stream(raw);
    std::string line;
    HttpRequest req;
    std::getline(stream, line);
    std::istringstream request_line(line);
    request_line >> req.method >> req.path;

    while (std::getline(stream, line) && line != "\r") {
        auto pos = line.find(": ");
        if (pos != std::string::npos) {
            req.headers[line.substr(0, pos)] = line.substr(pos + 2);
        }
    }

    std::string body;
    while (std::getline(stream, line)) {
        body += line + "\n";
    }
    req.body = body;

    return req;
}

HttpResponse handle_request(const HttpRequest& req) {
    if (req.method == "GET" && req.path == "/") {
        return {200, "OK", {{"Content-Type", "text/html"}}, "<h1>Hello</h1>"};
    }

    if (req.method == "GET" && req.path.starts_with("/static/")) {
        std::ifstream f("server/" + req.path.substr(8));
        if (!f) {
            return {404, "Not Found", {}, "not found"};
        }
        std::ostringstream content;
        content << f.rdbuf();
        return {200, "OK", {{"Content-Type", "text/plain"}}, content.str()};
    }

    if (req.method == "POST" && req.path == "/upload") {
        auto it = req.headers.find("filename");
        if (it == req.headers.end()) return {400, "Bad Request", {}, "missing filename"};

        std::ofstream f("server/" + it->second);
        if (!f) return {500, "Error", {}, "Could not write"};
        f << req.body;
        return {200, "OK", {}, "uploaded"};
    }

    return {404, "Not Found", {}, "route not found"};
}

// TODO:
// https://stackoverflow.com/questions/15752659/thread-pooling-in-c11#32593825
class ThreadPool {
   public:
    ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                while (true) {
                    int client_fd;
                    {
                        std::unique_lock lock(queue_mutex);
                        condition.wait(lock, [this] { return !tasks.empty() || stop; });
                        if (stop && tasks.empty()) return;
                        client_fd = tasks.front();
                        tasks.pop();
                    }
                    process_client(client_fd);
                }
            });
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& t : workers) t.join();
    }

    void enqueue(int client_fd) {
        {
            std::lock_guard lock(queue_mutex);
            tasks.push(client_fd);
        }
        condition.notify_one();
    }

   private:
    std::vector<std::thread> workers;
    std::queue<int> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;

    static void process_client(int client_fd) {
        char buffer[4096];
        int bytes = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes <= 0) {
            close(client_fd);
            return;
        }
        buffer[bytes] = '\0';

        std::string req_str(buffer);
        HttpRequest req = parse_request(req_str);
        HttpResponse res = handle_request(req);

        std::string res_str = res.to_string();
        send(client_fd, res_str.c_str(), res_str.size(), 0);
        close(client_fd);
    }
};

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return 1;
    }

    if (listen(server_fd, 50) < 0) {
        perror("listen failed");
        return 1;
    }

    std::cout << "http://localhost:8080\n";

    ThreadPool pool(std::thread::hardware_concurrency());

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            pool.enqueue(client_fd);
        }
    }

    close(server_fd);
    return 0;
}

