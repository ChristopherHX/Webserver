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
#ifdef _WIN32
		char * buf = new char[512];
	#ifdef _WIN32
		GetModuleFileNameA(NULL, buf, 512);
	#else
		readlink("/proc/self/exe", buf, 512);
	#endif
		fs::path p = buf;
		delete[] buf;
#ifdef __RPC_ARM32__
		p = p.parent_path();
#else
		p = p.parent_path().parent_path().parent_path().parent_path();
#endif
		fs::path web = "C:";
#else
	fs::path p = "/etc/letsencrypt/live/p4fdf5699.dip0.t-ipconnect.de";
	fs::path web = "/home/ubuntu/WebServer-C++/web";
#endif
		try {
			Http2::Server server(p, web);
			std::string command;
			std::getline(std::cin, command);
		}
		catch (std::exception &ex){
			std::cout << "Error:" << ex.what() << "\r\n";
		}
		return 0;
 //   Server server;
 //   std::string command = "start";
	//server.SetRootFolder("C:");
 //   //server.SetRootFolder(p.parent_path() / ".." / "web");
 //   server.OnRequest([](RequestArgs &args){
 //       std::cout << args.path << "\n";
 //       args.Progress = [](int prog){
 //           std::cout << "Progress:" << prog << "\r";
 //           if(prog == 100) std::cout << "\n";
 //       };
 //   });
 //   do
 //   {
 //       if(command == "start")
 //       {
 //           server.Starten(80, 443);
 //           std::cout << "Server gestartet.\n";
 //       }
 //       else if(command == "stop")
 //       {
 //           server.Stoppen();
 //           std::cout << "Server gestoppt.\n";
 //       }
	//	std::getline(std::cin, command);
 //   } while(command != "exit");
 //   server.Stoppen();
 //   std::cout << "Server beendet.\n";
 //   return 0;
}
