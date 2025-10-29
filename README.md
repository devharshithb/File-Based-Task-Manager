# 🗂️ File-Based Task Manager (C Project)

A simple multi-threaded **client–server Task Manager** implemented in **C using TCP sockets**.
It demonstrates network communication between sender and receiver nodes, persistent file-based storage, and thread-safe concurrent access.

---

## 📘 Overview

The **File-Based Task Manager** allows multiple clients to connect to a server and perform basic task management operations over the network.

Each client can:

* Add new tasks with description and priority
* List or search tasks
* Mark tasks as completed
* Delete or download all tasks

The server stores all tasks in a shared text file (`tasks.txt`) and logs every client action in `server_log.txt`.

---

## 🧩 Features

| Command            | Description                      |                                       |
| ------------------ | -------------------------------- | ------------------------------------- |
| `ADD <desc>        | <priority>`                      | Adds a new task with a priority level |
| `LIST`             | Displays all tasks from the file |                                       |
| `SEARCH <keyword>` | Finds tasks matching the keyword |                                       |
| `DONE <id>`        | Marks a task as completed        |                                       |
| `DELETE <id>`      | Deletes a task by ID             |                                       |
| `DOWNLOAD`         | Dumps all tasks to client        |                                       |
| `HELP`             | Shows command usage              |                                       |
| `QUIT`             | Safely disconnects the client    |                                       |

---

## 🧱 Architecture

```
┌────────────┐      TCP Socket      ┌─────────────┐
│   Client   │  <---------------->  │    Server   │
│ (Sender)   │                      │ (Receiver)  │
└────────────┘                      └─────────────┘
                                          │
                                          ▼
                                   ┌───────────────┐
                                   │ tasks.txt     │  ← stores tasks persistently
                                   ├───────────────|  
                                   │ server_log.txt|   ← logs all activity
                                   └───────────────┘
```

* **Server**: Uses `pthread` threads and mutex locks for concurrent access.
* **Client**: Interactive CLI for sending commands.
* **Protocol**: TCP ensures reliable data transfer.

---

## ⚙️ Setup and Compilation

### Prerequisites

* Ubuntu / WSL
* `gcc`, `make`, and `pthread` installed

### Clone or Create Project

```bash
cd ~/code
mkdir file_task_manager
cd file_task_manager
```

### Build

```bash
make
```

This compiles:

```
server  →  multi-threaded server
client  →  interactive client
```

---

## ▶️ Run the Program

### Step 1: Start Server

```bash
./server
```

### Step 2: Start Client (in another terminal)

```bash
./client
```

You’ll see:

```
==== FILE-BASED TASK MANAGER ====
1. Add Task
2. List Tasks
3. Search Tasks
4. Mark Task Done
5. Delete Task
6. Download All
7. Help
8. Quit
Select:
```

---

## 💃 File Structure

```
file_task_manager/
├── server.c
├── client.c
├── Makefile
├── tasks.txt
└── server_log.txt
```

---

## 🧠 Example

**Client:**

```
ADD Complete Assignment|High
LIST
DONE 1
QUIT
```

**Server Console:**

```
Server listening on port 8080 (IST)
[2025-10-29 22:45:11] ADD id=1 "Complete Assignment" High
[2025-10-29 22:46:03] DONE id=1
```

**tasks.txt**

```
1|Complete Assignment|Completed|High|2025-10-29 22:46:03
```

---

## 🗾 Logs

All actions are logged with timestamps in:

```
server_log.txt
```

Example:

```
[2025-10-29 22:40:18] ADD id=2 "Review Code" Medium
[2025-10-29 22:42:00] LIST
```

---

## 🛠️ Technical Stack

| Component       | Technology                  |
| --------------- | --------------------------- |
| Language        | C                           |
| Networking      | TCP sockets (`arpa/inet.h`) |
| Concurrency     | POSIX threads (`pthread.h`) |
| Synchronization | Mutex locks                 |
| Platform        | Ubuntu / WSL                |
| Build System    | GNU Make                    |

---

## 🧮 Future Enhancements

* Replace text files with SQLite or JSON.
* Add authentication for clients.
* Support multiple servers (load balancing).
* Add encryption for secure communication.

---

## 👨‍💻 Author

**Harshith B**

BE in Computer Science and Engineering
BMS College of Engineering

---

## 🩶 License

This project is released for educational purposes under the MIT License.
