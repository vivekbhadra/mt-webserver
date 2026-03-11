# mt-webserver

A production-grade POC of a **Multithreaded Web Server** in C++, containerised with Docker Compose and fronted by Nginx as a reverse proxy.

---

## Architecture

```
  Browser / curl
       │
       ▼ :80
  ┌─────────────────────────────┐
  │  Nginx (reverse proxy)      │
  │  - Rate limiting 30 req/s   │
  │  - Security headers         │
  └──────────────┬──────────────┘
                 │ :8080 (internal)
  ┌──────────────▼──────────────┐
  │  C++ Server                 │
  │                             │
  │  [Listener Thread]          │
  │       │ push(fd)            │
  │  [SafeQueue]  ← condvar     │
  │       │ pop(fd)             │
  │  [Worker Pool: 4 threads]   │
  │       │                     │
  │  [Connection Map] ← mutex   │
  │       ▲                     │
  │  [Console Thread]           │
  └─────────────────────────────┘

Docker network: webnet (bridge)
Port 8080 is internal only — never exposed to host
```

---

## Project Structure

```
mt-webserver/
├── src/
│   └── server.cpp          ← C++ server
├── nginx/
│   └── nginx.conf          ← conf.d drop-in config
├── Dockerfile              ← multi-stage build
├── docker-compose.yml      ← full stack
├── Makefile
├── .env.example
├── .gitignore
└── README.md
```

---

## Quick Start

```bash
cp .env.example .env
make up
```

Visit **http://localhost**

---

## Endpoints

| URL | Description |
|-----|-------------|
| `/` | Home — shows which worker thread handled it |
| `/status` | Server config |
| `/connections` | Live connection table |
| `/slow` | 3s delay — open 4 tabs to prove concurrency |
| `/health` | Nginx health check |

---

## Console Thread

```bash
make console        # attach to server stdin
> list              # show active connections
> stats             # queue depth + connection count
> quit              # graceful shutdown
```

Press `Ctrl+P Ctrl+Q` to detach without stopping.

---

## All Commands

```bash
make up             # build + start everything
make down           # stop and remove containers
make logs           # tail all logs
make restart        # restart C++ server only
make shell          # shell inside server container
make console        # attach to server stdin
make run            # build and run locally (no Docker)
```

---

## Testing Concurrency

```bash
# All 4 finish in ~3s, not 12s — thread pool in action
for i in {1..4}; do curl http://localhost/slow & done; wait
```

---

## Key C++ Concepts

| Concept | Where |
|---------|-------|
| `std::thread` | Worker pool, console thread |
| `std::mutex` + `std::lock_guard` | Connection map, SafeQueue |
| `std::condition_variable` | SafeQueue::pop() — blocks idle workers |
| `std::atomic<bool>` | `server_running` shutdown flag |
| Producer/consumer | Listener → Queue → Workers |
| Graceful shutdown | `wake_all()` + `join()` |

## Industry Practices

| Practice | How |
|----------|-----|
| Multi-stage Docker build | Slim runtime image, no compiler in prod |
| Non-root container user | `appuser` runs the binary |
| Health checks | Nginx waits for healthy server before starting |
| Nginx reverse proxy | App port never exposed publicly |
| Rate limiting | 30 req/s per IP |
| Security headers | X-Frame-Options, nosniff, server_tokens off |
| Log rotation | JSON driver, 10MB max, 3 files |
| Resource limits | CPU + memory caps in Compose |
| `.env` for config | No hardcoded values |
