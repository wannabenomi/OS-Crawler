#ifndef HTTP_UTILS_H
#define HTTP_UTILS_H

#include <string>
#include <vector>
#include <netdb.h> // For hostent structure

struct PageResult {
    std::string url;
    int status_code;
    std::string html_content;
    std::vector<std::string> links;
};

class HttpUtils {
public:
    static bool parseUrl(const std::string& raw_url, std::string& host, std::string& path);
    static PageResult downloadPage(const std::string& url);
    static std::vector<std::string> extractLinks(const std::string& html, const std::string& base_url);
};

#endif