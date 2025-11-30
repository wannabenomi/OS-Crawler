# 🕸️ Multi-Threaded OS Web Crawler (C++)

A robust, fault-tolerant **web crawler built from scratch in C++** to simulate an Operating System Kernel.

This project demonstrates core **Operating System concepts** including:

- **Process Management** (Threading)  
- **Synchronization** (Mutexes & Monitors)  
- **Deadlock Prevention**  
- **Context Switching** (State Persistence)  
- **Manual Memory & Resource Management**  

It uses **Raw TCP Sockets** and **OpenSSL** for networking — no libcurl or high-level HTTP libraries.

---

## 🚀 Key Features (OS Concepts)

| Feature | OS Concept Implemented |
|--------|-------------------------|
| **Thread Pool** | Process Management — 5 worker “processes” (threads) running concurrently |
| **UrlFrontier** | Monitor Pattern — Shared memory with Mutex + Condition Variables |
| **DNS Lock** | Critical Section — Protects the non-thread-safe `gethostbyname()` call |
| **State Saving** | Context Switching / Hibernation — Serializes RAM state on `SIGINT` |
| **Auto-Shutdown** | Idle Detection — Scheduler stops system after prolonged inactivity |

---

## 📂 File Structure

### 1. **The Kernel (Main Process)** — `main.cpp`
**Role:** Scheduler & Entry Point  
**Responsibilities:**
- Initialize the shared UrlFrontier  
- Spawn the 5-thread worker pool  
- Register `SIGINT` signal handler  
- Detect idle state and trigger safe shutdown  

---

### 2. **Shared Memory (The Monitor)** — `UrlFrontier.h / UrlFrontier.cpp`
**Role:** Thread-Safe Task Manager  

**Responsibilities:**
- Manage FIFO Task Queue  
- Maintain Visited Set (History)  
- Use Mutexes for exclusive access  
- Put worker threads to sleep using Condition Variables  
- Persist state (`saveData()` / `loadData()`)  
- Export unique links to `crawled_links.csv`  

---

### 3. **I/O Drivers (Networking)** — `HttpUtils.h / HttpUtils.cpp`
**Role:** Manual HTTP/HTTPS Client + Socket Manager  

**Features:**
- Raw TCP socket creation  
- Manual HTTP GET request crafting  
- OpenSSL-based HTTPS support  
- 3-second timeout to prevent deadlocks  
- ASCII-only domain filtering to prevent DNS hangs  

---

### 4. **Build System** — `Makefile`
Compiles and links:
- `pthread` (multithreading)
- `ssl` & `crypto` (OpenSSL)

---

## 🔄 System Flow (Lifecycle of a Thread)

### **1. Boot**
- Load previous state from `crawler_state.txt`
- If empty → seed with default initial URLs

### **2. Schedule**
Worker threads start and wait for tasks.

### **3. Fetch (Critical Section)**
- Lock Frontier Mutex  
- If queue empty → thread sleeps  
- Pop a URL  
- Unlock Mutex  

### **4. Execute (I/O Bound)**
- Parse URL  
- Acquire *DNS Mutex* → resolve hostname  
- Connect via TCP or SSL  
- Download HTML  

### **5. Parse (CPU Bound)**
- Extract `<a href="...">` links  
- Filter and validate them  

### **6. Commit (Critical Section)**
- Lock Frontier Mutex  
- Check for duplicates  
- Push new unique links into queue  
- Notify sleeping threads  

### **7. Interrupt / Shutdown**
Triggered when:
- User presses **Ctrl + C**, or  
- Queue stays empty for >4 seconds  

The crawler:
- Serializes RAM state  
- Saves into `crawler_state.txt`  
- Exits safely  

---

## 🛠️ How to Compile & Run

### **Prerequisites (Linux / WSL)**

```bash
sudo apt-get update
sudo apt-get install build-essential libssl-dev

## 🛠️ Compilation

```bash
make
./crawler
