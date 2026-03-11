#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>

struct Connection
{
    int         fd;
    std::string remote_ip;
    int         remote_port;
    std::string status; // "active" | "done"
    std::chrono::steady_clock::time_point started;
};

class ConnectionManager
{
  public:
    void   register_connection(int fd, const std::string &ip, int port);
    void   close_connection(int fd);
    void   list(std::ostream &out) const;
    size_t count() const;

    // Expose mutex + map for RequestHandler's /connections page
    mutable std::mutex        conn_mutex;
    std::map<int, Connection> connections;
};
