#include "ConnectionManager.hpp"
#include "RequestHandler.hpp"
#include "WebServer.hpp"

int main()
{
    ConnectionManager cm;
    RequestHandler    rh(cm);
    WebServer         ws(cm, rh);
    ws.start();
    return 0;
}
