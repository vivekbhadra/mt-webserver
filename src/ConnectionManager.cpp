#include "ConnectionManager.hpp"

#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

void ConnectionManager::register_connection(int fd, const std::string &ip, int port)
{
    std::lock_guard<std::mutex> lk(conn_mutex);
    connections[fd] = { fd, ip, port, "active", std::chrono::steady_clock::now() };
}

void ConnectionManager::close_connection(int fd)
{
    shutdown(fd, SHUT_RDWR);
    ::close(fd);
    std::lock_guard<std::mutex> lk(conn_mutex);
    connections.erase(fd);
}

void ConnectionManager::list(std::ostream &out) const
{
    std::lock_guard<std::mutex> lk(conn_mutex);
    if (connections.empty())
    {
        out << "[console] No active connections.\n";
        return;
    }
    for (auto &[fd, c] : connections)
        out << "  fd=" << fd << " " << c.remote_ip << ":" << c.remote_port << " [" << c.status << "]\n";
}

size_t ConnectionManager::count() const
{
    std::lock_guard<std::mutex> lk(conn_mutex);
    return connections.size();
}
