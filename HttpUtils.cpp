#include "HttpUtils.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <sys/time.h>
#include <mutex> 
#include <cctype> 

bool HttpUtils::parseUrl(const std::string& raw_url, std::string& host, std::string& path) {
    std::string clean_url = raw_url;
    
    // STRICT CHECK: We only support HTTP
    const std::string prefix = "http://";
    if (clean_url.substr(0, prefix.size()) != prefix) {
        // --- NEW MODIFICATION ---
        // std::cout << "[Warning] Skipped invalid protocol: " << raw_url << std::endl;
        return false; 
    }
    
    clean_url = clean_url.substr(prefix.size());

    // SECURITY CHECK: Reject non-ASCII characters
    for (unsigned char c : clean_url) {
        if (!isascii(c)) {
            // --- NEW MODIFICATION ---
            std::cout << "[Security] Blocked unsafe URL: " << raw_url << std::endl;
            return false;
        }
    }

    size_t slash_pos = clean_url.find('/');
    if (slash_pos != std::string::npos) {
        host = clean_url.substr(0, slash_pos);
        path = clean_url.substr(slash_pos);
    } else {
        host = clean_url;
        path = "/";
    }
    return !host.empty();
}

PageResult HttpUtils::downloadPage(const std::string& url) {
    PageResult result;
    result.url = url;
    result.status_code = 0;

    std::string host, path;
    if (!parseUrl(url, host, path)) {
        return result; 
    }

    // --- CRITICAL SECTION START ---
    static std::mutex dns_lock; 
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);

    {
        std::lock_guard<std::mutex> guard(dns_lock);
        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            std::cerr << "[Error] DNS lookup failed for: " << host << std::endl;
            return result;
        }
        std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    }
    // --- CRITICAL SECTION END ---

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return result;

    struct timeval timeout;
    timeout.tv_sec = 3; 
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        // Silent fail on connect is okay, but you can uncomment to debug
        // std::cerr << "[Error] Connection failed to: " << host << std::endl;
        close(sock);
        return result;
    }

    std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nUser-Agent: OSCrawler/1.0\r\nConnection: close\r\n\r\n";
    send(sock, request.c_str(), request.length(), 0);

    char buffer[4096];
    std::string response;
    int bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }
    close(sock);

    size_t header_end = response.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        result.html_content = response.substr(header_end + 4);
        result.status_code = 200;
        result.links = extractLinks(result.html_content, host);
    }
    return result;
}

std::vector<std::string> HttpUtils::extractLinks(const std::string& html, const std::string& host) {
    std::vector<std::string> links;
    std::string search = "href=\"";
    size_t pos = 0;
    while ((pos = html.find(search, pos)) != std::string::npos) {
        pos += search.length();
        size_t end = html.find("\"", pos);
        if (end != std::string::npos) {
            std::string link = html.substr(pos, end - pos);
            if (link.find("http://") == 0) {
                links.push_back(link);
            }
            pos = end;
        }
    }
    return links;
}