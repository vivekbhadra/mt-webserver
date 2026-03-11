#include "ConsoleThread.hpp"

#include <iostream>

ConsoleThread::ConsoleThread(ConnectionManager &cm, SafeQueue<int> &queue, std::atomic<bool> &running)
    : cm_(cm), queue_(queue), running_(running)
{
}

void ConsoleThread::run()
{
    std::cout << "\n[console] Commands: list | stats | quit\n> " << std::flush;
    std::string line;
    while (running_ && std::getline(std::cin, line))
    {
        if (line == "list")
        {
            cm_.list(std::cout);
        }
        else if (line == "stats")
        {
            std::cout << "[console] Queue: " << queue_.size() << " | Connections: " << cm_.count() << "\n";
        }
        else if (line == "quit" || line == "exit")
        {
            std::cout << "[console] Shutting down...\n";
            running_ = false;
            queue_.wake_all();
            break;
        }
        else if (!line.empty())
        {
            std::cout << "[console] Unknown: " << line << "\n";
        }
        if (running_)
            std::cout << "> " << std::flush;
    }
}
