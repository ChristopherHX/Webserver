#include "HttpServer.h"
#include "Http2.h"
#include <iostream>
#include <string>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

using namespace Http;
namespace fs = std::experimental::filesystem;

int main(int argc, const char** argv)
{
		char * buf = new char[512];
	#ifdef _WIN32
		GetModuleFileNameA(NULL, buf, 512);
	#else
		readlink("/proc/self/exe", buf, 512);
	#endif
		fs::path p = buf;
		delete[] buf;
		p = p.parent_path();
		//std::fstream fs(p.parent_path() / "Serverlog.txt");
		try {
			//fs << "Server wird gestartet..\r\n";
			Http2::Server server;
			server.Start(p);
			//fs << "Server ist gestartet..\r\n";
			std::string command;
			std::getline(std::cin, command);
			//fs << "Server wurde beendet durch:\"" << command << "\r\n";
		}
		catch (std::exception &ex){
			//fs << "Error:" << ex.what() << "\r\n";
			//fs.close();
		}
		return 0;
	
//    Server server;
//    std::string command = "start";

//    server.SetRootFolder(p.parent_path() / ".." / "web");
//    server.OnRequest([](RequestArgs &args){
//        std::cout << args.path << "\n";
//        args.Progress = [](int prog){
//            std::cout << "Progress:" << prog << "\r";
//            if(prog == 100) std::cout << "\n";
//        };
//    });
//    do
//    {
//        if(command == "start")
//        {
//            server.Starten(80, 443);
//            std::cout << "Server gestartet.\n";            
//        }
//        else if(command == "stop")
//        {
//            server.Stoppen();
//            std::cout << "Server gestoppt.\n";
//        }
//		std::getline(std::cin, command);
//    } while(command != "exit");
//    server.Stoppen();
//    std::cout << "Server beendet.\n";
//    return 0;
}