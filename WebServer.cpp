#include "HttpServer.h"
#include "Http2.h"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::experimental::filesystem;

int main(int argc, const char** argv)
{
#ifdef _WIN32
	auto _cicp = GetConsoleCP(), _cocp = GetConsoleOutputCP();
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
#endif
	try
	{
		int cmd = 0;
		fs::path executablefolder = fs::canonical(fs::path(*argv++).parent_path()), privatekey = executablefolder / "privkey.pem", publiccertificate = executablefolder / "publicchain", webroot = executablefolder;
		--argc;
		while (argc > 0)
		{
			if (strcmp(*argv, "-h1") == 0)
			{
				++argv;
				--argc;
				cmd = 1;
			}
			else if (strcmp(*argv, "-h2") == 0)
			{	
				++argv;
				--argc;
				cmd = 2;
			}
			else if (strcmp(*argv, "-prikey") == 0)
			{
				++argv;
				privatekey = *argv++;
				argc -= 2;
			}
			else if (strcmp(*argv, "-pubcert") == 0)
			{
				++argv;
				publiccertificate = *argv++;
				argc -= 2;
			}
			else if (strcmp(*argv, "-webroot") == 0)
			{
				++argv;
				webroot = *argv++;
				argc -= 2;
			}
		}
		switch (cmd)
		{
		case 1:
		{
			Http::Server server(privatekey, publiccertificate, webroot);
			std::string command;
			//server.OnRequest([](RequestArgs &args) {
			//	std::cout << args.path << "\n";
			//	args.Progress = [](int prog) {
			//		std::cout << "Progress:" << prog << "\r";
			//		if (prog == 100) std::cout << "\n";
			//	};
			//});
			do
			{
				std::getline(std::cin, command);
			} while (command != "exit");
			break;
		}
		case 2:
		{
			Http2::Server server(privatekey, publiccertificate, webroot);
			std::string command;
			do
			{
				std::getline(std::cin, command);
			} while (command != "exit");
			break;
		}
		default:
			std::cout << u8"Benutzung vom Server\n";
			std::cout << u8"-h1 HTTP/1.1 Server\n";
			std::cout << u8"-h2 HTTP/2.0 Server\n";
			std::cout << u8"-prikey <pfad> privater Schlüssel\n";
			std::cout << u8"-pubcert <pfad> öffentliches Zertifikat\n";
			std::cout << u8"-webroot <pfad> Webroot\n";
			break;
		}
	}
	catch (std::exception &ex) {
		std::cout << "Fehler:" << ex.what() << "\r\n";
	}
#ifdef _WIN32
	SetConsoleCP(_cicp);
	SetConsoleOutputCP(_cocp);
#endif
	return 0;
}