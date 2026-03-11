#include "WebServer.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static const int PORT        = 8080;
static const int THREAD_POOL = 4;
static const int BACKLOG     = 10;

WebServer::WebServer(ConnectionManager &cm, RequestHandler &rh)
    : cm_(cm), rh_(rh)
{
}

void WebServer::start()
{
    if (!setup_socket())
        return;

    std::cout << "═══════════════════════════════════════\n"
              << "  Multithreaded Web Server POC\n"
              << "  Listening on http://localhost:" << PORT << "\n"
              << "  Thread pool size: " << THREAD_POOL << "\n"
              << "═══════════════════════════════════════\n";

    for (int i = 0; i < THREAD_POOL; i++)
        pool_.emplace_back(&WebServer::worker, this, i);

    ConsoleThread ct(cm_, queue_, running_);
    std::thread   console(&ConsoleThread::run, &ct);

    accept_loop();

    close(server_fd_);
    queue_.wake_all();
    for (auto &t : pool_)
        t.join();
    if (console.joinable())
        console.join();

    std::cout << "[main] Server stopped cleanly.\n";
}

bool WebServer::setup_socket()
{
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) { perror("socket"); return false; }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(server_fd_, (sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return false; }
    if (listen(server_fd_, BACKLOG) < 0)                       { perror("listen"); return false; }

    return true;
}

void WebServer::accept_loop()
{
    while (running_)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd_, &fds);
        struct timeval tv{ 1, 0 };
        if (select(server_fd_ + 1, &fds, nullptr, nullptr, &tv) <= 0)
            continue;

        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);
        int         client_fd  = accept(server_fd_, (sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
            continue;

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        int port = ntohs(client_addr.sin_port);

        std::cout << "[listener] Accepted " << ip << ":" << port << " fd=" << client_fd << "\n";
        cm_.register_connection(client_fd, ip, port);
        queue_.push(client_fd);
    }
}

void WebServer::worker(int id)
{
    std::cout << "[worker-" << id << "] started\n";
    int client_fd;
    while (queue_.pop(client_fd, running_))
    {
        std::cout << "[worker-" << id << "] handling fd=" << client_fd << "\n";
        rh_.handle(client_fd);
    }
    std::cout << "[worker-" << id << "] exiting\n";
}
