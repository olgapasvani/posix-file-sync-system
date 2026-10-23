# File Synchronization System (FSS)

A robust, multi-process directory synchronization system implemented in C. This project demonstrates advanced POSIX system programming concepts, including real-time file system monitoring, Inter-Process Communication (IPC), and concurrent process management.

**System Architecture & Logic**
* **Manager (`fss_manager.c` & `manager_functions.c`)**: The central daemon. It initializes by reading directory pairs from the configuration file into a `sync_info` structure and performs an initial synchronization. It then uses `poll()` to concurrently multiplex between:
  1. File system events via `inotify` (monitoring modifications and creations).
  2. User commands via named pipes (`fss_in`).
* **Console (`fss_console.c`)**: An interactive CLI that communicates with the manager.
* **Worker (`fss_worker.c`)**: Short-lived child processes spawned to handle actual file operations concurrently.

**Available Console Commands**
* add <source_dir> <target_dir>: Dynamically adds a new directory pair to the system for real-time monitoring and synchronization.
* status <directory>: Displays the current status, the timestamp of the last synchronization, and the total error count for the specified directory.
* sync <directory>: Manually triggers an immediate synchronization for the specified directory.
* cancel <directory>: Stops monitoring the specified directory (removes the inotify watch) and removes it from the system.
* shutdown: Gracefully terminates the manager, closes the console, and waits for all active worker processes to finish their tasks before completely exiting. 

**How to Compile and Run**
Compile the project using the provided Makefile:
```bash
make

Run the manager:
./fss_manager -l manager_logfile -c config_file -n 6