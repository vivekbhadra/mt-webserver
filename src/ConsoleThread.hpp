#pragma once

#include "ConnectionManager.hpp"
#include "SafeQueue.hpp"

#include <atomic>

class ConsoleThread
{
  public:
    ConsoleThread(ConnectionManager &cm, SafeQueue<int> &queue, std::atomic<bool> &running);
    void run();

  private:
    ConnectionManager  &cm_;
    SafeQueue<int>     &queue_;
    std::atomic<bool>  &running_;
};
