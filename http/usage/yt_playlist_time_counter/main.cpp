#include <cpr/cpr.h>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

const std::string API_KEY = "";
const std::string API_URL = "https://www.googleapis.com/youtube/v3/playlistItems";
const std::string VIDEO_DETAILS_URL = "https://www.googleapis.com/youtube/v3/videos";

std::string extractPlaylistID(const std::string& url) {
  std::regex re(R"([?&]list=([a-zA-Z0-9_-]+))");
  std::smatch match;
  if (std::regex_search(url, match, re) && match.size() > 1) {
    return match[1].str();
  }
  return "";
}

std::vector<std::string> fetchPlaylistItems(const std::string& playlistID) {
  std::vector<std::string> videoIDs;
  std::string nextPageToken = "";

  do {
    cpr::Response r = cpr::Get(cpr::Url{API_URL},
                               cpr::Parameters{{"part", "contentDetails"},
                                               {"playlistId", playlistID},
                                               {"maxResults", "50"},
                                               {"pageToken", nextPageToken},
                                               {"key", API_KEY}});
    if (r.status_code != 200) {
      throw std::runtime_error("Failed to fetch playlist items");
    }

    auto json = nlohmann::json::parse(r.text);
    for (const auto& item : json["items"]) {
      videoIDs.push_back(item["contentDetails"]["videoId"].get<std::string>());
    }

    if (json.contains("nextPageToken")) {
      nextPageToken = json["nextPageToken"].get<std::string>();
    } else {
      nextPageToken = "";
    }
  } while (!nextPageToken.empty());

  return videoIDs;
}

std::chrono::seconds parseISO8601Duration(const std::string& duration) {
  std::regex re(R"(PT(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)");
  std::smatch match;
  if (std::regex_match(duration, match, re)) {
    int hours = match[1].matched ? std::stoi(match[1].str()) : 0;
    int minutes = match[2].matched ? std::stoi(match[2].str()) : 0;
    int seconds = match[3].matched ? std::stoi(match[3].str()) : 0;
    return std::chrono::seconds(hours * 3600 + minutes * 60 + seconds);
  }
  return std::chrono::seconds(0);
}

std::chrono::seconds fetchVideoDuration(const std::string& videoID) {
  cpr::Response r = cpr::Get(cpr::Url{VIDEO_DETAILS_URL}, cpr::Parameters{
                                             {"part", "contentDetails"},
                                             {"id", videoID},
                                             {"key", API_KEY}});
  if (r.status_code != 200) {
    throw std::runtime_error("Failed to fetch video details");
  }

  auto json = nlohmann::json::parse(r.text);
  if (json["items"].empty()) {
    throw std::runtime_error("No video details found for ID " + videoID);
  }

  std::string duration = json["items"][0]["contentDetails"]["duration"].get<std::string>();
  return parseISO8601Duration(duration);
}

std::string formatDuration(std::chrono::seconds duration) {
  int hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
  int minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count() % 60;
  int seconds = duration.count() % 60;
  std::ostringstream oss;
  oss << hours << "h " << minutes << "m " << seconds << "s";
  return oss.str();
}

void printDurationAtSpeed(std::chrono::seconds totalDuration, double speed) {
  // <https://en.cppreference.com/w/cpp/chrono/duration/duration_cast>
  auto adjustedDuration = std::chrono::duration_cast<std::chrono::seconds>(totalDuration / speed);
  std::cout << "At " << std::fixed << std::setprecision(2) << speed << "x: "
            << formatDuration(adjustedDuration) << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "usage: ytplaycount <playlist_url>" << std::endl;
    return 1;
  }

  std::string playlistURL = argv[1];
  std::string playlistID = extractPlaylistID(playlistURL);
  if (playlistID.empty()) {
    std::cerr << "Invalid YouTube playlist URL" << std::endl;
    return 1;
  }

  std::cout << "\nfetching: " << playlistID << "..." << std::endl;

  try {
    auto videoIDs = fetchPlaylistItems(playlistID);

    std::chrono::seconds totalDuration(0);
    for (const auto& videoID : videoIDs) {
      try {
        totalDuration += fetchVideoDuration(videoID);
      } catch (const std::exception& e) {
        std::cerr << "error fetching video duration for " << videoID << ": " << e.what() << std::endl;
      }
    }

    std::cout << "\nTotal duration: " << formatDuration(totalDuration) << "\n\n";

    printDurationAtSpeed(totalDuration, 1.25);
    printDurationAtSpeed(totalDuration, 1.5);
    printDurationAtSpeed(totalDuration, 1.75);
    printDurationAtSpeed(totalDuration, 2.0);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
