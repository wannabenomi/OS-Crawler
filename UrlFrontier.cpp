#include "UrlFrontier.h"
#include <iostream>

UrlFrontier::UrlFrontier() : finished(false) {}

void UrlFrontier::push(const std::string& url) {
    // 1. Acquire Lock
    std::lock_guard<std::mutex> lock(mtx);
    
    // 2. Check if already visited
    if (visited.find(url) == visited.end()) {
        visited.insert(url);
        taskQueue.push(url);
        
        // 3. Notify one waiting worker that data is available
        cv.notify_one(); 
    }
}

bool UrlFrontier::pop(std::string& url) {
    // 1. Acquire Unique Lock (allows unlocking while waiting)
    std::unique_lock<std::mutex> lock(mtx);
    
    // 2. WAIT CONDITION: Sleep while queue is empty AND not finished
    // This prevents "Busy Waiting" (burning CPU for no reason)
    cv.wait(lock, [this]{ return !taskQueue.empty() || finished; });
    
    // 3. Exit condition
    if (taskQueue.empty() && finished) {
        return false; 
    }
    
    // 4. Critical Section: actually take the item
    url = taskQueue.front();
    taskQueue.pop();
    
    return true;
}

int UrlFrontier::getSize() {
    std::lock_guard<std::mutex> lock(mtx);
    return taskQueue.size();
}

void UrlFrontier::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        finished = true;
    }
    // Wake up EVERYONE so they can see the 'finished' flag and exit
    cv.notify_all();
}

// ... keep previous functions exactly the same ...

// DUMP MEMORY TO DISK
void UrlFrontier::saveData(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mtx); // Lock everything so no thread changes it while we write
    
    std::ofstream file(filename);
    if (!file.is_open()) return;

    // 1. Save Visited Set
    file << visited.size() << "\n";
    for (const auto& url : visited) {
        file << url << "\n";
    }

    // 2. Save Queue
    // (std::queue doesn't allow iteration, so we copy it to save)
    std::queue<std::string> copy = taskQueue;
    file << copy.size() << "\n";
    while (!copy.empty()) {
        file << copy.front() << "\n";
        copy.pop();
    }
    
    std::cout << "[System] State saved to " << filename << std::endl;
    file.close();
}

// RESTORE MEMORY FROM DISK
void UrlFrontier::loadData(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mtx);
    
    std::ifstream file(filename);
    if (!file.is_open()) return;

    std::string line;
    int count;

    // 1. Load Visited
    if (file >> count) {
        std::getline(file, line); // consume newline
        for (int i = 0; i < count; ++i) {
            std::getline(file, line);
            if (!line.empty()) visited.insert(line);
        }
    }

    // 2. Load Queue
    if (file >> count) {
        std::getline(file, line); // consume newline
        for (int i = 0; i < count; ++i) {
            std::getline(file, line);
            if (!line.empty()) taskQueue.push(line);
        }
    }
    
    std::cout << "[System] Restored " << taskQueue.size() << " tasks from disk." << std::endl;
    file.close();
}