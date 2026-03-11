#include "RequestHandler.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

static const int PORT        = 8080;
static const int THREAD_POOL = 4;

RequestHandler::RequestHandler(ConnectionManager &cm) : cm_(cm) {}

void RequestHandler::handle(int fd)
{
    std::string request = read_request(fd);
    if (request.empty())
    {
        cm_.close_connection(fd);
        return;
    }

    std::string method, path, version;
    std::istringstream(request) >> method >> path >> version;

    std::string response = route(path);
    send_all(fd, response);
    cm_.close_connection(fd);
}

// ─── Private ──────────────────────────────────────────────────────────────────

std::string RequestHandler::read_request(int fd)
{
    struct timeval tv{ 5, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string req;
    char        buf[4096] = {};
    while (req.find("\r\n\r\n") == std::string::npos)
    {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0)
            return "";
        req.append(buf, n);
        if (req.size() > 8192)
            return "";
    }
    return req;
}

bool RequestHandler::send_all(int fd, const std::string &data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        ssize_t n = send(fd, data.c_str() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return false;
        sent += n;
    }
    return true;
}

std::string RequestHandler::build_response(int code, const std::string &status, const std::string &body,
                                           const std::string &ctype)
{
    std::ostringstream oss;
    oss << "HTTP/1.1 " << code << " " << status << "\r\n"
        << "Content-Type: " << ctype << "; charset=utf-8\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    return oss.str();
}

std::string RequestHandler::route(const std::string &path)
{
    if (path == "/")
    {
        std::string body = "<html><body>"
                           "<h1>Multithreaded Web Server POC</h1>"
                           "<p>Worker thread ID: <b>" +
                           std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()) % 10000) +
                           "</b></p><ul>"
                           "<li><a href='/status'>/status</a></li>"
                           "<li><a href='/connections'>/connections</a></li>"
                           "<li><a href='/slow'>/slow</a> - 3s delay, open 4 tabs to test concurrency</li>"
                           "</ul></body></html>";
        return build_response(200, "OK", body);
    }

    if (path == "/status")
    {
        std::ostringstream s;
        s << "<html><body><h2>Server Status</h2>"
          << "<p>Thread pool size: " << THREAD_POOL << "</p>"
          << "<p>Port: " << PORT << "</p></body></html>";
        return build_response(200, "OK", s.str());
    }

    if (path == "/connections")
    {
        std::ostringstream s;
        s << "<html><body><h2>Active Connections</h2>"
          << "<table border='1' cellpadding='4'>"
          << "<tr><th>FD</th><th>IP</th><th>Port</th><th>Status</th></tr>";
        {
            std::lock_guard<std::mutex> lk(cm_.conn_mutex);
            for (auto &[cfd, c] : cm_.connections)
                s << "<tr><td>" << cfd << "</td><td>" << c.remote_ip << "</td><td>" << c.remote_port
                  << "</td><td>" << c.status << "</td></tr>";
        }
        s << "</table></body></html>";
        return build_response(200, "OK", s.str());
    }

    if (path == "/slow")
    {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::string body = "<html><body><h2>Slow endpoint done (3s)</h2>"
                           "<p>If 4 tabs all returned together, the thread pool is working.</p>"
                           "</body></html>";
        return build_response(200, "OK", body);
    }

    return build_response(404, "Not Found", "<html><body><h2>404 Not Found</h2></body></html>");
}
