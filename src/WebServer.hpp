#pragma once

#include "ConnectionManager.hpp"
#include "ConsoleThread.hpp"
#include "RequestHandler.hpp"
#include "SafeQueue.hpp"

#include <atomic>
#include <thread>
#include <vector>

class WebServer
{
  public:
    WebServer(ConnectionManager &cm, RequestHandler &rh);
    void start();

  private:
    ConnectionManager &cm_;
    RequestHandler    &rh_;

    SafeQueue<int>           queue_;
    std::atomic<bool>        running_{ true };
    std::vector<std::thread> pool_;
    int                      server_fd_{ -1 };

    void worker(int id);
    bool setup_socket();
    void accept_loop();
};
