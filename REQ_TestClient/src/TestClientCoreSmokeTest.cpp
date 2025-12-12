#include <req/clientcore/ClientCore.h>
#include <iostream>

//
// Simple compile/link smoke test only – not called yet.
//
void TestClientCoreSmoke()
{
    using namespace req::clientcore;

    ClientConfig config{};
    ClientSession session{};

    std::cout << "[ClientCore] Smoke test – default client version: "
              << config.clientVersion << std::endl;

    // Do not actually call login() yet; servers may not be running.
}
