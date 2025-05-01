#include <curl/curl.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

using json = nlohmann::json;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: github-activity <username>" << std::endl;
        return 1;
    }

    std::string username = argv[1];
    std::string api_url = "https://api.github.com/users/" + username + "/events";

    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "github-activity-cpp");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return 1;
        }

        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if (response_code != 200) {
            std::cerr << "Error: Could not fetch activity "
                         "for user '"
                      << username << "'. Response code: " << response_code << std::endl;
            curl_easy_cleanup(curl);
            return 1;
        }

        curl_easy_cleanup(curl);
    }

    try {
        auto json_response = json::parse(readBuffer);

        if (!json_response.is_array()) {
            if (json_response.contains("message")) {
                std::cerr << "Error: " << json_response["message"] << std::endl;
            } else {
                std::cerr << "Unknown error occurred" << std::endl;
            }
            return 1;
        }

        for (const auto& event : json_response) {
            std::string event_type = event.value("type", "Unknown");
            std::string repo_name = event["repo"].value("name", "Unknown");

            if (event_type == "PushEvent") {
                int commit_count = event["payload"].value("size", 0);
                std::cout << "- Pushed " << commit_count << " commits to " << repo_name << std::endl;
            } else if (event_type == "IssuesEvent") {
                std::string action = event["payload"].value("action", "");
                if (action == "opened") {
                    std::cout << "- Opened a new issue in " << repo_name << std::endl;
                } else if (action == "closed") {
                    std::cout << "- Closed an issue in " << repo_name << std::endl;
                }
            } else if (event_type == "WatchEvent") {
                std::cout << "- Starred " << repo_name << std::endl;
            } else {
                std::cout << "- Unhandled event type: " << event_type << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
        return 1;
    }

    curl_global_cleanup();
    return 0;
}

