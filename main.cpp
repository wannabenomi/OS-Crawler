#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <csignal>
#include <atomic>
#include "HttpUtils.h"
#include "UrlFrontier.h"

UrlFrontier frontier;
std::atomic<bool> keepRunning(true); 

void signalHandler(int signum) {
    std::cout << "\n\n!!! INTERRUPT RECEIVED (Signal " << signum << ") !!!" << std::endl;
    keepRunning = false;
    frontier.shutdown(); 
}

void crawlerWorker(int id) {
    std::string url;
    while (frontier.pop(url)) {
        if (!keepRunning) break;

        std::cout << "[Thread " << id << "] Downloading: " << url << std::endl;
        PageResult result = HttpUtils::downloadPage(url);

        if (result.status_code == 200) {
            // LOG SUCCESS
            std::cout << "[Thread " << id << "] DONE " << url 
                      << " (" << result.links.size() << " links)" << std::endl;
            
            for (const auto& link : result.links) {
                frontier.push(link);
            }
        } else {
            // LOG FAILURE
             std::cout << "[Thread " << id << "] FAILED " << url << std::endl;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// ... keep imports and worker function the same ...

int main() {
    signal(SIGINT, signalHandler);

    std::cout << "--- OS CRAWLER STARTED ---" << std::endl;
    std::cout << "Press Ctrl+C to stop and save state." << std::endl;

    frontier.loadData("crawler_state.txt");

    if (frontier.getSize() == 0) {
        std::cout << "[System] Queue empty. Seeding..." << std::endl;
        frontier.push("http://crawler-test.com/");
        // frontier.push("http://info.cern.ch/"); 
    }

    int thread_count = 5;
    std::vector<std::thread> pool;
    for (int i = 0; i < thread_count; ++i) {
        pool.emplace_back(crawlerWorker, i + 1);
    }

    // --- NEW MODIFICATION: AUTO-STOP LOGIC ---
    int idle_counter = 0;
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        if (frontier.getSize() == 0) {
            idle_counter++;
            // If queue is empty for 4 seconds, assume we are done
            if (idle_counter >= 4) {
                std::cout << "\n[System] Queue is empty and threads are idle." << std::endl;
                std::cout << "[System] Auto-shutdown initiated..." << std::endl;
                keepRunning = false;
                frontier.shutdown();
            }
        } else {
            idle_counter = 0; // Reset counter if new links appeared
        }
    }

    std::cout << "--- SAVING STATE ---" << std::endl;
    for (auto& t : pool) {
        if (t.joinable()) t.join();
    }
    
    frontier.saveData("crawler_state.txt");
    std::cout << "Goodbye." << std::endl;

    return 0;
}