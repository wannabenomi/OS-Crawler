#ifndef URL_FRONTIER_H
#define URL_FRONTIER_H

#include <queue>
#include <string>
#include <set>
#include <mutex>
#include <condition_variable>
#include <fstream> // REQUIRED for File I/O

class UrlFrontier {
private:
    std::queue<std::string> taskQueue;
    std::set<std::string> visited;
    
    std::mutex mtx;
    std::condition_variable cv;
    bool finished;

public:
    UrlFrontier();
    
    void push(const std::string& url);
    bool pop(std::string& url);
    int getSize();
    void shutdown();

    // --- NEW PERSISTENCE METHODS ---
    void saveData(const std::string& filename);
    void loadData(const std::string& filename);
    
    // --- NEW: EXPORT TO CSV ---
    void exportToCSV(const std::string& filename);
};

#endif