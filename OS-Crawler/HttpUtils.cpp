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

// OPENSSL HEADERS
#include <openssl/ssl.h>
#include <openssl/err.h>

// Helper to check for HTTPS vs HTTP
bool HttpUtils::parseUrl(const std::string& raw_url, std::string& host, std::string& path) {
    std::string clean_url = raw_url;
    bool is_https = false;

    // Check Protocol
    if (clean_url.substr(0, 7) == "http://") {
        clean_url = clean_url.substr(7);
        is_https = false;
    } else if (clean_url.substr(0, 8) == "https://") {
        clean_url = clean_url.substr(8);
        is_https = true;
    } else {
        // Unknown protocol
        return false;
    }

    // Security Check (ASCII only)
    for (unsigned char c : clean_url) {
        if (!isascii(c)) {
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
    
    // We return true if valid. The caller will determine port based on is_https logic 
    // (We can't easily return is_https here without changing the header, 
    //  so we'll re-check the raw string in downloadPage for simplicity).
    return !host.empty();
}

PageResult HttpUtils::downloadPage(const std::string& url) {
    PageResult result;
    result.url = url;
    result.status_code = 0;

    std::string host, path;
    if (!parseUrl(url, host, path)) return result;

    // Determine Port & Protocol
    bool use_ssl = (url.substr(0, 8) == "https://");
    int port = use_ssl ? 443 : 80;

    // --- 1. DNS RESOLUTION (Critical Section) ---
    static std::mutex dns_lock;
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    {
        std::lock_guard<std::mutex> guard(dns_lock);
        struct hostent* server = gethostbyname(host.c_str());
        if (server == nullptr) {
            // std::cerr << "[Error] DNS failed: " << host << std::endl;
            return result;
        }
        std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    }

    // --- 2. SOCKET CREATION ---
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return result;

    struct timeval timeout;
    timeout.tv_sec = 3; 
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    // --- 3. TCP CONNECT ---
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return result;
    }

    // --- 4. PREPARE REQUEST ---
    std::string request = "GET " + path + " HTTP/1.1\r\n"
                          "Host: " + host + "\r\n"
                          "User-Agent: OSCrawler/1.0\r\n"
                          "Connection: close\r\n\r\n";
    
    std::string response;
    char buffer[4096];
    int bytes;

    // --- 5. SEND/RECEIVE (Branching Logic) ---
    if (use_ssl) {
        // === HTTPS PATH (OpenSSL) ===
        
        // Init Context (Ideally done once globally, but okay here for assignment)
        SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
        if (!ctx) { close(sock); return result; }

        // Create SSL Object
        SSL* ssl = SSL_new(ctx);
        SSL_set_fd(ssl, sock);

        // Perform Handshake
        if (SSL_connect(ssl) <= 0) {
            // std::cerr << "[Error] SSL Handshake failed: " << host << std::endl;
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sock);
            return result;
        }

        // Send Encrypted
        SSL_write(ssl, request.c_str(), request.length());

        // Read Encrypted
        while ((bytes = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            response += buffer;
        }

        // Cleanup SSL
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);

    } else {
        // === HTTP PATH (Raw Socket) ===
        
        send(sock, request.c_str(), request.length(), 0);
        while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';
            response += buffer;
        }
    }

    close(sock);

    // --- 6. PARSE BODY ---
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
            
            // FILTER: Now we accept both http AND https
            if (link.find("http://") == 0 || link.find("https://") == 0) {
                links.push_back(link);
            }
            pos = end;
        }
    }
    return links;
}