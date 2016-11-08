#include "HttpServer.h"

#include <iostream>
#include <string>

using namespace HttpServer;

int main(int argc, const char** argv)
{
    Server server;
    std::string command = "";
    server.SetRootFolder("./Web");
    server.ShowFolder(false);
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