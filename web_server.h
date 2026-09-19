#pragma once

namespace WebServer
{
    bool Start(int preferredPort);
    void Stop();
    int Port();
}
