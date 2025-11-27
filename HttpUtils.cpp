#include "HttpUtils.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <sys/time.h> // Required for timeout

bool HttpUtils::parseUrl(const std::string& raw_url, std::string& host, std::string& path) {
    std::string clean_url = raw_url;
    
    // STRICT CHECK: We only support HTTP
    const std::string prefix = "http://";
    if (clean_url.substr(0, prefix.size()) != prefix) {
        return false; // Reject HTTPS or FTP
    }
    
    clean_url = clean_url.substr(prefix.size());

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
        // Silently fail for non-http urls
        return result; 
    }

    // 1. DNS Resolution
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        // std::cerr << "[Error] DNS lookup failed for: " << host << std::endl;
        return result;
    }

    // 2. Create Socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return result;

    // --- OS FIX: ADD TIMEOUT (Prevent Zombie Threads) ---
    struct timeval timeout;
    timeout.tv_sec = 3;  // 3 Seconds timeout
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    // ----------------------------------------------------

    // 3. Connect
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return result;
    }

    // 4. Send HTTP Request
    std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\nUser-Agent: OSCrawler/1.0\r\nConnection: close\r\n\r\n";
    send(sock, request.c_str(), request.length(), 0);

    // 5. Receive Response
    char buffer[4096];
    std::string response;
    int bytes;
    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }
    close(sock);

    // 6. Extract Body
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
            
            // FILTER: Only allow strictly http:// links
            if (link.find("http://") == 0) {
                links.push_back(link);
            }
            pos = end;
        }
    }
    return links;
}