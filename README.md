# Multithreaded Web Server — POC

A minimal C++ web server demonstrating:
- **Thread pool** (worker threads consuming from a shared queue)
- **Producer/consumer pattern** (listener → queue → workers)
- **Mutex-protected connection tracking**
- **Console management thread** (live commands while server runs)
- **Condition variables** for efficient thread wake-up

---

## Project Structure

```
multithreaded-webserver/
├── src/
│   └── server.cpp      ← all logic in one file
├── Dockerfile
├── Makefile
└── README.md
```

---

## Architecture

```
[Browser/curl]
      │  TCP connect
      ▼
[Listener Thread]  ← main thread, accept() loop
      │  push(client_fd)
      ▼
[SafeQueue<int>]   ← mutex + condition_variable protected
      │  pop(client_fd)
      ▼
[Worker Threads 0–3]  ← thread pool, handle HTTP
      │  register/remove
      ▼
[Connection Map]   ← mutex protected, fd → Connection info
      ▲
[Console Thread]   ← reads stdin: list / stats / quit
```

---

## Option A: Run Locally on Ubuntu

### Prerequisites
```bash
sudo apt-get install build-essential g++
```

### Build & Run
```bash
make run
```

Server starts at **http://localhost:8080**

---

## Option B: Run with Docker

### Build image
```bash
make docker-build
# or: docker build -t mt-webserver .
```

### Run container
```bash
make docker-run
# or: docker run -it --rm -p 8080:8080 mt-webserver
```

> `-it` keeps stdin open so you can use the console thread commands

Server starts at **http://localhost:8080**

---

## Endpoints

| URL              | Description                                      |
|------------------|--------------------------------------------------|
| `/`              | Home page — shows which worker thread handled it |
| `/status`        | Server config info                               |
| `/connections`   | Live connection table                            |
| `/slow`          | 3-second delay — open multiple tabs to prove concurrency |

---

## Console Commands (while server is running)

```
> list     — show active connections
> stats    — queue depth + connection count
> quit     — graceful shutdown
```

---

## Testing Concurrency

Open **4+ browser tabs** to `/slow` simultaneously — they will all complete in ~3s because the thread pool handles them in parallel. Without multithreading, they'd take 3s × N sequentially.

```bash
# Or use curl in parallel:
for i in {1..4}; do curl http://localhost:8080/slow & done; wait
```

---

## Key C++ Concepts Illustrated

| Concept                  | Where in code                        |
|--------------------------|--------------------------------------|
| `std::thread`            | Worker pool, console thread          |
| `std::mutex`             | Connection map, SafeQueue            |
| `std::lock_guard`        | All mutex acquisitions               |
| `std::condition_variable`| SafeQueue::pop() — blocks workers    |
| `std::atomic<bool>`      | `server_running` flag                |
| Producer/consumer        | Listener pushes, workers pop         |
| Graceful shutdown        | `wake_all()` + join()                |
