#include "mal-fetcher.hpp"
#include <curl/curl.h>
#include <map>
#include <obs-module.h>

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static void replace_all(std::string &str, const std::string &from, const std::string &to)
{
    if (from.empty()) return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

MALFetcher::MALFetcher(const std::string &username) : username_(username) {}

std::string MALFetcher::fetchPage(const std::string &url)
{
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            blog(LOG_ERROR, "curl_easy_perform() failed: %s", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    
    return readBuffer;
}

std::string MALFetcher::extractDataItems(const std::string &html)
{
    const std::string key = "data-items=";
    size_t pos = html.find(key);
    if (pos == std::string::npos) return "";

    // find the first quote after the key
    size_t quote_pos = html.find_first_of("\"'", pos + key.size());
    if (quote_pos == std::string::npos) return "";
    char quote = html[quote_pos];

    // find the matching closing quote
    size_t end_pos = html.find(quote, quote_pos + 1);
    if (end_pos == std::string::npos || end_pos <= quote_pos + 1) return "";

    return html.substr(quote_pos + 1, end_pos - quote_pos - 1);
}

std::string MALFetcher::normalizeImageUrl(const std::string &url)
{
    if (url.empty()) {
        return "https://cdn.myanimelist.net/images/qm_50.gif";
    }
    
    std::string normalized = url;

    auto replace_once = [](std::string &s, const std::string &from, const std::string &to) {
        size_t pos = s.find(from);
        if (pos != std::string::npos) s.replace(pos, from.size(), to);
    };

    replace_once(normalized, "/r/96x136", "");
    replace_once(normalized, "/r/50x70", "");
    
    // Add https if protocol-relative
    if (normalized.substr(0, 2) == "//") {
        normalized = "https:" + normalized;
    }
    
    // Add full CDN URL if path-only
    if (!normalized.empty() && normalized[0] == '/' && normalized.substr(0, 2) != "//") {
        normalized = "https://cdn.myanimelist.net" + normalized;
    }
    
    return normalized;
}

std::vector<MALEntry> MALFetcher::fetchList(const std::string &status, const std::string &media)
{
    std::vector<MALEntry> entries;
    
    // Status code mapping
    std::map<std::string, std::string> statusMap;
    if (media == "manga") {
        statusMap = {{"READING", "1"}, {"COMPLETED", "2"}, {"PAUSED", "3"}, 
                     {"DROPPED", "4"}, {"PLANNING", "6"}, {"ALL", "7"}};
    } else {
        statusMap = {{"WATCHING", "1"}, {"COMPLETED", "2"}, {"PAUSED", "3"}, 
                     {"DROPPED", "4"}, {"PLANNING", "6"}, {"ALL", "7"}};
    }
    
    std::string statusCode = statusMap.count(status) ? statusMap[status] : "7";
    std::string url = "https://myanimelist.net/" + media + "list/" + username_ + "?status=" + statusCode;
    
    blog(LOG_INFO, "Fetching MAL list: %s", url.c_str());
    
    std::string html = fetchPage(url);
    if (html.empty()) {
        blog(LOG_ERROR, "Failed to fetch page");
        return entries;
    }
    
    std::string dataItems = extractDataItems(html);
    if (dataItems.empty()) {
        blog(LOG_ERROR, "Could not find data-items in page");
        return entries;
    }
    
    try {
        // Unescape HTML entities (no regex to avoid recursion/stack issues)
        std::string unescaped = dataItems;
        replace_all(unescaped, "&quot;", "\"");
        replace_all(unescaped, "&amp;", "&");
        replace_all(unescaped, "&lt;", "<");
        replace_all(unescaped, "&gt;", ">");
        
        auto json = nlohmann::json::parse(unescaped);
        
        for (const auto &item : json) {
            MALEntry entry;
            
            std::string idKey = (media == "manga") ? "manga_id" : "anime_id";
            std::string titleKey = (media == "manga") ? "manga_title" : "anime_title";
            std::string imageKey = (media == "manga") ? "manga_image_path" : "anime_image_path";
            std::string progressKey = (media == "manga") ? "num_read_chapters" : "num_watched_episodes";
            
            entry.id = std::to_string(item[idKey].get<int>());
            entry.title = item[titleKey].get<std::string>();
            entry.coverImage = normalizeImageUrl(item[imageKey].get<std::string>());
            entry.status = status;
            entry.progress = item.value(progressKey, 0);
            entry.media = media;
            
            entries.push_back(entry);
        }
        
        blog(LOG_INFO, "Fetched %zu entries", entries.size());
        
    } catch (const std::exception &e) {
        blog(LOG_ERROR, "Failed to parse JSON: %s", e.what());
    }
    
    return entries;
}
