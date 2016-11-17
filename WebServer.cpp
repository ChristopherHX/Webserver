#include "HttpServer.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace HttpServer;
namespace fs = std::experimental::filesystem;

int main(int argc, const char** argv)
{
    Server server;
    std::string command = "";
	char * buf = new char[512];
#ifdef _WIN32
	GetModuleFileNameA(NULL, buf, 512);
#else
	readlink("/proc/self/exe", buf, 512);
#endif
	fs::path p = buf;
	delete[] buf;
    server.SetRootFolder(p.parent_path() / ".." / "web");
     server.Request = [](RequestArgs * args){
         std::cout << args->path << "\n";
         args->Progress = [](int prog){
             std::cout << "Progress:" << prog << "\r";
             if(prog == 100) std::cout << "\n";
         };
     };
    do
    {
        std::getline(std::cin, command);
        if(command == "start")
        {
            server.Starten(80, 443);
            std::cout << "Server gestartet.\n";            
        }
        else if(command == "stop")
        {
            server.Stoppen();
            std::cout << "Server gestoppt.\n";
        }
    } while(command != "exit");
    server.Stoppen();
    std::cout << "Server beendet.\n";
    return 0;
}