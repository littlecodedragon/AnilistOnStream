#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

struct MALEntry {
    std::string id;
    std::string title;
    std::string coverImage;
    std::string status;
    int progress;
    std::string media;
};

class MALFetcher {
public:
    MALFetcher(const std::string &username);
    
    std::vector<MALEntry> fetchList(const std::string &status, const std::string &media);
    
    static std::string normalizeImageUrl(const std::string &url);
    
private:
    std::string username_;
    
    std::string fetchPage(const std::string &url);
    std::string extractDataItems(const std::string &html);
};
