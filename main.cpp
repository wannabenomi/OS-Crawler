#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <csignal> // For Signal Handling
#include <atomic>
#include "HttpUtils.h"
#include "UrlFrontier.h"

UrlFrontier frontier;
std::atomic<bool> keepRunning(true); // Atomic flag for the main loop

// THE SIGNAL HANDLER (Interrupt Service Routine)
void signalHandler(int signum) {
    std::cout << "\n\n!!! INTERRUPT RECEIVED (Signal " << signum << ") !!!" << std::endl;
    keepRunning = false;
    frontier.shutdown(); // Wake up threads so they can exit
}

void crawlerWorker(int id) {
    std::string url;
    while (frontier.pop(url)) {
        // If signal received, stop working immediately
        if (!keepRunning) break;

        std::cout << "[Thread " << id << "] Downloading: " << url << std::endl;
        PageResult result = HttpUtils::downloadPage(url);

        if (result.status_code == 200) {
            // std::cout << "[Thread " << id << "] Found " << result.links.size() << " links." << std::endl;
            for (const auto& link : result.links) {
                frontier.push(link);
            }
        }
        
        // Small delay to make logs readable
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    // 1. REGISTER SIGNAL HANDLER (Ctrl+C)
    signal(SIGINT, signalHandler);

    std::cout << "--- OS CRAWLER STARTED ---" << std::endl;
    std::cout << "Press Ctrl+C to stop and save state." << std::endl;

    // 2. Load previous state if exists
    frontier.loadData("crawler_state.txt");

    // If queue is empty (fresh start), seed it
    if (frontier.getSize() == 0) {
        frontier.push("http://crawler-test.com/");
        frontier.push("http://example.com/");
    }

    // 3. Spawn Threads
    int thread_count = 5;
    std::vector<std::thread> pool;
    for (int i = 0; i < thread_count; ++i) {
        pool.emplace_back(crawlerWorker, i + 1);
    }

    // 4. Main Loop (Waits for Ctrl+C)
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (frontier.getSize() == 0 && keepRunning) {
             std::cout << "[System] Queue empty. Waiting for threads..." << std::endl;
        }
    }

    // 5. CLEANUP & SAVE
    std::cout << "--- SAVING STATE ---" << std::endl;
    for (auto& t : pool) {
        if (t.joinable()) t.join();
    }
    
    frontier.saveData("crawler_state.txt"); // <--- PERSISTENCE
    std::cout << "Goodbye." << std::endl;

    return 0;
}