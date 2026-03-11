#pragma once

#include "ConnectionManager.hpp"
#include <string>

class RequestHandler
{
  public:
    explicit RequestHandler(ConnectionManager &cm);
    void handle(int fd);

  private:
    ConnectionManager &cm_;

    std::string read_request(int fd);
    bool        send_all(int fd, const std::string &data);
    std::string build_response(int code, const std::string &status, const std::string &body,
                               const std::string &ctype = "text/html");

    std::string route(const std::string &path);
};
