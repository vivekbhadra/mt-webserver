# mt-webserver

A production-grade POC of a **Multithreaded Web Server** in C++, containerised with Docker and fronted by Nginx as a reverse proxy.

---

## Architecture

```
                        ┌─────────────────────────────────┐
  Browser / curl ──────▶│  Nginx (port 80)                │
                        │  - Reverse proxy                │
                        │  - Rate limiting (30 req/s)     │
                        │  - Security headers             │
                        │  - Logging                      │
                        └────────────┬────────────────────┘
                                     │ proxy_pass :8080
                        ┌────────────▼────────────────────┐
                        │  C++ Server (port 8080)         │
                        │                                 │
                        │  [Listener Thread]              │
                        │       │ push(fd)                │
                        │  [SafeQueue]  ◀── mutex+condvar │
                        │       │ pop(fd)                 │
                        │  [Worker Pool: 4 threads]       │
                        │       │                         │
                        │  [Connection Map] ◀── mutex     │
                        │       ▲                         │
                        │  [Console Thread] stdin cmds    │
                        └─────────────────────────────────┘

Docker network: webnet (bridge)
```

---

## Project Structure

```
mt-webserver/
├── src/
│   └── server.cpp          ← C++ server (all logic)
├── nginx/
│   └── nginx.conf          ← reverse proxy config
├── Dockerfile              ← multi-stage build
├── docker-compose.yml      ← full stack definition
├── Makefile                ← dev shortcuts
├── .env.example            ← environment template
├── .gitignore
└── README.md
```

---

## Quick Start

```bash
# 1. Clone / enter project
cd mt-webserver

# 2. Copy env
cp .env.example .env

# 3. Build and run everything
make up
```

That's it. Two containers start:
- **mt-webserver** (C++ server on :8080, internal only)
- **mt-webserver-nginx** (Nginx on :80, public)

---

## Accessing the Server

| URL | Description |
|-----|-------------|
| `http://localhost/` | Home — shows which worker thread handled request |
| `http://localhost/status` | Server config |
| `http://localhost/connections` | Live connection table |
| `http://localhost/slow` | 3s delay — open 4 tabs to prove concurrency |
| `http://localhost/health` | Nginx health check |

> Traffic always goes through Nginx on port 80. Port 8080 is internal to the Docker network.

---

## Console Thread

The console thread reads commands from stdin while the server runs.

```bash
# Attach to the server container's stdin
make console
# or: docker attach mt-webserver

> list     # show active connections
> stats    # queue depth + connection count
> quit     # graceful shutdown
```

Press `Ctrl+P Ctrl+Q` to detach without stopping the container.

---

## Useful Commands

```bash
make up          # build images + start all services
make down        # stop and remove containers
make logs        # tail logs from all services
make restart     # restart just the C++ server
make shell       # open a shell inside the server container
make build       # rebuild images without starting

# Local build (no Docker, Ubuntu only)
make run
```

---

## Testing Concurrency

```bash
# Open 4 slow requests in parallel — all finish in ~3s, not 12s
for i in {1..4}; do curl http://localhost/slow & done; wait
```

---

## Key Concepts Illustrated

| Concept | Where |
|---------|-------|
| `std::thread` | Worker pool, console thread |
| `std::mutex` + `std::lock_guard` | Connection map, SafeQueue |
| `std::condition_variable` | SafeQueue::pop() — blocks idle workers |
| `std::atomic<bool>` | `server_running` shutdown flag |
| Producer/consumer pattern | Listener → Queue → Workers |
| Graceful shutdown | `wake_all()` + `join()` |
| Multi-stage Docker build | Slim runtime image, no build tools in prod |
| Nginx reverse proxy | Rate limiting, headers, upstream routing |
| Docker Compose | Full stack as code, single command startup |

---

## Industry Practices Used

- **Multi-stage Dockerfile** — builder stage compiles, runtime stage is minimal
- **Non-root container user** — `appuser` runs the binary, not root
- **Docker health checks** — Compose waits for healthy server before starting Nginx
- **Nginx reverse proxy** — never expose app server directly in production
- **Rate limiting** — 30 req/s per IP via Nginx `limit_req`
- **Security headers** — X-Frame-Options, X-Content-Type-Options, server_tokens off
- **Structured logging** — JSON log driver with rotation
- **Resource limits** — CPU and memory caps in Compose
- **`.env` for config** — no hardcoded values, `.env.example` for onboarding
- **`.gitignore`** — secrets and binaries excluded from version control
