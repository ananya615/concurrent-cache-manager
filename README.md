# A Concurrent Cache Manager

## Project Topic
A Concurrent Cache Manager: Implement a fixed-size in-memory cache (key-value store) that can be accessed by multiple threads. Use reader-writer locks (`pthread_rwlock_t`) to allow concurrent reads but exclusive access for writes and cache evictions.

---

## Project Description
This project implements a concurrent, thread-safe in-memory cache in C, designed to efficiently handle multiple readers and writers.  
It uses POSIX threads (`pthread`) and reader-writer locks (`pthread_rwlock_t`) to allow multiple concurrent reads while ensuring exclusive access for writes and cache evictions.

When the cache reaches its maximum capacity, it automatically removes the Least Recently Used (LRU) entry to make space for new data.  
The project demonstrates synchronization, thread safety, and efficient memory management in a multithreaded environment.

---

## Features
- Fixed-size key-value cache
- Thread-safe access using reader-writer locks
- Concurrent read operations
- Exclusive write and eviction access
- Automatic LRU (Least Recently Used) eviction policy
- Demonstration of multithreading and synchronization in C

---

## Technologies Used
- Language: C  
- Libraries: `pthread.h`, `stdio.h`, `stdlib.h`, `string.h`, `time.h`  
- Synchronization: `pthread_rwlock_t` (reader-writer locks)

---

## How to Compile and Run
1. Open a terminal in the project directory.
2. Compile the program:
   ```bash
   gcc cache_manager.c -pthread -o cache_manager

## Run the executable:
./cache_manager

## Expected output:
Test completed successfully!

