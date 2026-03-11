#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// POSIX socket headers
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// ─────────────────────────────────────────────
// CONFIG
// ─────────────────────────────────────────────
static const int PORT = 8080;      // Listening port
static const int THREAD_POOL = 4;  // Number of worker threads
static const int BACKLOG = 10;     // Max pending connections in listen queue
static const int RECV_TIMEOUT = 5; // seconds

// ─────────────────────────────────────────────
// CONNECTION TRACKER
// ─────────────────────────────────────────────
struct Connection
{
    int fd;
    std::string remote_ip;                         // For display only; not used for logic
    int remote_port;                               // For display only; not used for logic
    std::string status;                            // "active" | "done"
    std::chrono::steady_clock::time_point started; // time_point when connection was accepted
};

std::mutex conn_mutex;
std::map<int, Connection> connections; // fd → Connection
std::atomic<int> next_conn_id{ 1 };

void register_connection(int fd, const std::string &ip, int port)
{
    std::lock_guard<std::mutex> lk(conn_mutex);
    connections[fd] = { fd, ip, port, "active", std::chrono::steady_clock::now() };
}

void close_connection(int fd)
{
    // shutdown is a linux-specific way to signal the client we're done before closing
    // shutdown(fd, SHUT_WR) would allow client to read remaining data before we close the socket
    // shutdown is better than just close() for TCP sockets to avoid RST (Reset) issues on some
    // platforms when client is still reading
    shutdown(fd, SHUT_RDWR);
    ::close(fd);
    std::lock_guard<std::mutex> lk(conn_mutex);
    connections.erase(fd); // Remove from connection tracking map
}

// ─────────────────────────────────────────────
// THREAD-SAFE WORK QUEUE  (Producer/Consumer)
// ─────────────────────────────────────────────
template <typename T> class SafeQueue
{
    std::queue<T> task_queue;
    std::mutex mtx;
    std::condition_variable cv;

  public:
    void push(T item)
    {
        {
            std::lock_guard<std::mutex> lk(mtx);
            task_queue.push(std::move(item));
        }
        cv.notify_one();
    }

    // Blocking pop; returns false if shut down with empty queue
    bool pop(T &out, std::atomic<bool> &running)
    {
        std::unique_lock<std::mutex> lk(mtx);
        // Wait until there's work or we're shutting down
        cv.wait(lk, [&] { return !task_queue.empty() || !running; });
        if (task_queue.empty())
            return false;
        out = std::move(task_queue.front());
        task_queue.pop();
        return true;
    }

    void wake_all()
    {
        cv.notify_all();
    }
    size_t size()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return task_queue.size();
    }
};

// ─────────────────────────────────────────────
// HTTP HANDLER
// ─────────────────────────────────────────────
std::string build_response(int status_code, const std::string &status_text, const std::string &body,
                           const std::string &content_type = "text/html")
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;
    return oss.str();
}

void handle_client(int fd)
{
    // Set receive timeout
    struct timeval tv
    {
        RECV_TIMEOUT, 0
    };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string request;
    ssize_t n{ 0 };
    char buf[4096] = {};
    while (request.find("\r\n\r\n") == std::string::npos)
    {
        n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
        {
            close_connection(fd);
            return;
        }
        request.append(buf, n);
        if (request.size() > 8192)
        {
            close_connection(fd);
            return;
        }
    }

    // std::string request(buf, n);
    //  Parse first line: METHOD PATH VERSION
    std::string method, path, version;
    {
        std::istringstream iss(request);
        iss >> method >> path >> version;
    }

    std::string body, response;

    if (path == "/")
    {
        body = "<html><body>"
               "<h1>🚀 Multithreaded Web Server POC</h1>"
               "<p>Thread: <b>" +
               std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000) +
               "</b></p>"
               "<ul>"
               "<li><a href='/status'>/status</a> — server status</li>"
               "<li><a href='/connections'>/connections</a> — active connections</li>"
               "<li><a href='/slow'>/slow</a> — slow endpoint (tests concurrency)</li>"
               "</ul></body></html>";
        response = build_response(200, "OK", body);
    }
    else if (path == "/status")
    {
        std::ostringstream s;
        s << "<html><body><h2>Server Status</h2>"
          << "<p>Thread pool size: " << THREAD_POOL << "</p>"
          << "<p>Port: " << PORT << "</p>"
          << "</body></html>";
        response = build_response(200, "OK", s.str());
    }
    else if (path == "/connections")
    {
        std::ostringstream s;
        s << "<html><body><h2>Active Connections</h2><table border=1>"
          << "<tr><th>FD</th><th>IP</th><th>Port</th><th>Status</th></tr>";
        {
            std::lock_guard<std::mutex> lk(conn_mutex);
            for (auto &[cfd, c] : connections)
            {
                s << "<tr><td>" << cfd << "</td><td>" << c.remote_ip << "</td><td>" << c.remote_port << "</td><td>"
                  << c.status << "</td></tr>";
            }
        }
        s << "</table></body></html>";
        response = build_response(200, "OK", s.str());
    }
    else if (path == "/slow")
    {
        // Simulate slow work — proves thread pool handles concurrency
        std::this_thread::sleep_for(std::chrono::seconds(3));
        body = "<html><body><h2>Slow endpoint done (3s delay)</h2></body></html>";
        response = build_response(200, "OK", body);
    }
    else
    {
        body = "<html><body><h2>404 Not Found</h2></body></html>";
        response = build_response(404, "Not Found", body);
    }

    size_t sent = 0;
    while (sent < response.size())
    {
        ssize_t n = send(fd, response.c_str() + sent, response.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            break;
        sent += n;
    }
    close_connection(fd);
}

// ─────────────────────────────────────────────
// THREAD POOL
// ─────────────────────────────────────────────
SafeQueue<int> work_queue; // queue of client fds
std::atomic<bool> server_running{ true };

void worker_thread(int id)
{
    std::cout << "[worker-" << id << "] started\n";
    int client_fd;
    while (work_queue.pop(client_fd, server_running))
    {
        std::cout << "[worker-" << id << "] handling fd=" << client_fd << "\n";
        handle_client(client_fd);
    }
    std::cout << "[worker-" << id << "] exiting\n";
}

// ─────────────────────────────────────────────
// CONSOLE THREAD
// ─────────────────────────────────────────────
void console_thread()
{
    std::cout << "\n[console] Commands: list | stats | quit\n> ";
    std::string line;
    while (server_running && std::getline(std::cin, line))
    {
        if (line == "list")
        {
            std::lock_guard<std::mutex> lk(conn_mutex);
            if (connections.empty())
            {
                std::cout << "[console] No active connections.\n";
            }
            else
            {
                for (auto &[fd, c] : connections)
                    std::cout << "  fd=" << fd << " " << c.remote_ip << ":" << c.remote_port << " [" << c.status
                              << "]\n";
            }
        }
        else if (line == "stats")
        {
            std::cout << "[console] Queue depth: " << work_queue.size() << " | Connections tracked: ";
            {
                std::lock_guard<std::mutex> lk(conn_mutex);
                std::cout << connections.size();
            }
            std::cout << "\n";
        }
        else if (line == "quit" || line == "exit")
        {
            std::cout << "[console] Shutting down...\n";
            server_running = false;
            work_queue.wake_all();
            break;
        }
        else if (!line.empty())
        {
            std::cout << "[console] Unknown command: " << line << "\n";
        }
        if (server_running)
            std::cout << "> ";
    }
}

// ─────────────────────────────────────────────
// MAIN — LISTENER THREAD
// ─────────────────────────────────────────────
int main()
{
    // Create listening socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        return 1;
    }

    std::cout << "═══════════════════════════════════════\n"
              << "  Multithreaded Web Server POC\n"
              << "  Listening on http://localhost:" << PORT << "\n"
              << "  Thread pool size: " << THREAD_POOL << "\n"
              << "═══════════════════════════════════════\n";

    // Spawn worker thread pool
    std::vector<std::thread> pool;
    for (int i = 0; i < THREAD_POOL; i++)
        pool.emplace_back(worker_thread, i);

    // Spawn console thread
    std::thread console(console_thread);

    // Listener loop (main thread)
    while (server_running)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        // Non-blocking accept check with timeout
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        struct timeval tv
        {
            1, 0
        };
        int ready = select(server_fd + 1, &fds, nullptr, nullptr, &tv);
        if (ready <= 0)
            continue;

        int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            continue;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        int port = ntohs(client_addr.sin_port);

        std::cout << "[listener] Accepted " << ip_str << ":" << port << " → fd=" << client_fd << "\n";

        register_connection(client_fd, ip_str, port);
        work_queue.push(client_fd);
    }

    // Cleanup
    close(server_fd);
    work_queue.wake_all();
    for (auto &t : pool)
        t.join();
    if (console.joinable())
        console.join();

    std::cout << "[main] Server stopped cleanly.\n";
    return 0;
}
