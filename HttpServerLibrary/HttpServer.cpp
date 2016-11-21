#include "HttpServer.h"

#include "ByteArray.h"
#include "HttpRequestBuffer.h"
#include "HttpHeader.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#undef max
#define SHUT_RDWR SD_BOTH
#define libext ".dll"
#define dlopen(name) LoadLibraryW(name)
#define dlsym(lib, symbol) GetProcAddress(lib, symbol)
#define dlclose(lib) FreeLibrary(lib)
#define extsymbol "?seite@@YAHAEAVRequestBuffer@Http@@@Z"
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/signal.h>
#include <dlfcn.h>
#define closesocket(socket) close(socket)
#define libext ".so"
#define dlopen(name) dlopen(name, RTLD_LAZY)
#define extsymbol "_Z5seiteRN4Http13RequestBufferE"
#endif
#include <ctime>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstring>
#include <cmath>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <list>

#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace Http;
namespace fs = std::experimental::filesystem;

Server::Server()
{
	servermainstop.store(false);
}

Server::~Server()
{
	Stoppen();
}

void Server::Starten(const int httpPort, const int httpsPort)
{
	if (servermainstop.load())
		return;
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(WINSOCK_VERSION, &wsaData);
#endif
	SSL_CTX* ctx = nullptr;

	fs::path certroot =
#ifdef _WIN32
		"../../"
#else
		"/etc/letsencrypt/live/p4fdf5699.dip0.t-ipconnect.de/"
#endif
		;
	fs::path publicchain = certroot / "fullchain.pem";
	fs::path privatekey = certroot / "privkey.pem";

	httpServerSocket = -1;
	httpsServerSocket = -1;

	if (fs::is_regular_file(publicchain) && fs::is_regular_file(privatekey))
	{
		SSL_load_error_strings();
		OpenSSL_add_ssl_algorithms();

		ctx = SSL_CTX_new(TLS_server_method());
		
		if (SSL_CTX_use_certificate_chain_file(ctx, publicchain.u8string().data()) < 0) {
			int i;
			ERR_print_errors_cb([](const char *str, size_t len, void *u) -> int {
				if ((*((int*)u)) == 0)
				{
					(*((int*)u))++;
					throw ServerException(std::string(str, len));
				}
			}, &i);
		}

		if (SSL_CTX_use_PrivateKey_file(ctx, privatekey.u8string().data(), SSL_FILETYPE_PEM) < 0) {
			int i;
			ERR_print_errors_cb([](const char *str, size_t len, void *u) -> int {
				if ((*((int*)u)) == 0)
				{
					(*((int*)u))++;
					throw ServerException(std::string(str, len));
				}
			}, &i);
			return;
		}
		CreateServerSocket(httpsServerSocket, httpsPort > 0 ? httpsPort : 433);
	}
	else
	{
		std::cerr << "Zertifikat(e) nicht gefunden\n";
		return;
	}

	CreateServerSocket(httpServerSocket, httpPort > 0 ? httpPort : 80);
	servermainstop.store(true);
	servermain = (void*)new std::thread([this, ctx]() {
		int nfds = std::max(httpServerSocket != -1 ? httpServerSocket : 0, httpsServerSocket != -1 ? httpsServerSocket : 0) + 1;
		fd_set sockets;
		timeval timeout;
		sockaddr_in clientAdresse;
		socklen_t clientAdresse_len = sizeof(clientAdresse);
		FD_ZERO(&sockets);
		while (servermainstop.load())
		{
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			if (httpServerSocket != -1)
			{
				FD_SET(httpServerSocket, &sockets);
			}
			if (httpsServerSocket != -1)
			{
				FD_SET(httpsServerSocket, &sockets);
			}
			select(nfds, &sockets, nullptr, nullptr, &timeout);
			bool Secure = FD_ISSET(httpsServerSocket, &sockets);
			if(Secure || FD_ISSET(httpServerSocket, &sockets))
			{
				uintptr_t clientSocket = accept(Secure ? httpsServerSocket : httpServerSocket, (sockaddr *)&clientAdresse, &clientAdresse_len);
				if (clientSocket != -1)
				{
					SSL *ssl = nullptr;
					if(Secure)
					{
						ssl = SSL_new(ctx);
						SSL_set_fd(ssl, clientSocket);
						SSL_accept(ssl);
					}
					ByteArray client(clientAdresse_len + 13);
					snprintf(client.Data(), client.Length(), "http%s://%s:%hu", ssl == nullptr ? "" : "s", inet_ntop(AF_INET, &clientAdresse.sin_addr, ByteArray(clientAdresse_len).Data(), clientAdresse_len), ntohs(clientAdresse.sin_port));
					std::thread(&Server::processRequest, this, std::unique_ptr<RequestBuffer>(new RequestBuffer(clientSocket, 1 << 14, std::string(client.Data()), ssl, rootfolder))).detach();
				}
			}
		}
		if(ctx != nullptr) SSL_CTX_free(ctx);
	});
}

void Server::Stoppen()
{
	if (!servermainstop.load())
		return;
	servermainstop.store(false);
	CloseSocket(httpServerSocket);
	CloseSocket(httpsServerSocket);
	((std::thread*)servermain)->join();
#ifdef _WIN32
	WSACleanup();
#endif
	delete (std::thread*)servermain;
}

void Server::CreateServerSocket(uintptr_t &serverSocket, int port)
{
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1)
	{
		throw ServerException(u8"Kann den Socket nicht erstellen");
	}
	sockaddr_in serverAdresse;
	serverAdresse.sin_family = AF_INET;
	serverAdresse.sin_addr.s_addr = INADDR_ANY;
	serverAdresse.sin_port = htons(port);
	if (bind(serverSocket, (sockaddr *)&serverAdresse, sizeof(serverAdresse)) == -1)
	{
		throw ServerException(u8"Kann den Socket nicht Binden");
	}
	if (listen(serverSocket, 10) == -1)
	{
		throw ServerException(u8"Kann den Socket nicht Abhören!\n");
	}
}

void Server::CloseSocket(uintptr_t &serverSocket)
{
	shutdown(serverSocket, SHUT_RDWR);
	closesocket(serverSocket);
	serverSocket = -1;
}

void Server::SetRootFolder(const fs::path &path)
{
	rootfolder = fs::canonical(path);
}

void Server::OnRequest(std::function<void(RequestArgs&)> onrequest)
{
	this->onrequest = onrequest;
}

void Server::processRequest(std::unique_ptr<RequestBuffer> buffer)
{
	using namespace std::chrono_literals;
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	RequestArgs status;
	std::cout << buffer->Client() << " Verbunden\n";
	buffer->RecvData([&status, &OnRequest = onrequest](RequestBuffer &buffer) -> int
	{
		Response &response = buffer.Response();
		response.Clear();
		response["Content-Length"] = "0";
		ByteArray buf(128);
		try
		{
			int header = buffer.IndexOf("\r\n\r\n", 0);
			if (header > 0)
			{
				header += 4;
				auto &reqest = buffer.Request() = Request(std::string(buffer.begin(), buffer.begin() + header));
				buffer.Free(header);
				status.path = buffer.Client() + " -> " + reqest.methode + ":" + reqest.request;
				if (OnRequest) OnRequest(status);
				if (!buffer.isSecure())
				{
					response.status = MovedPermanently;
					response["Connection"]["close"];
					response["Location"]["https://" + (reqest.Exists("Host") ? reqest["Host"].toString() : "p4fdf5699.dip0.t-ipconnect.de") + reqest.request];
					buffer.Send(response.toString());
					return -1;
				}
				else
				{
					response["Strict-Transport-Security"]["max-age"] = "31536000";
					response["Strict-Transport-Security"]["includeSubDomains"];
					response["Connection"] = "keep-alive";
					if (reqest.request == "/")
					{
						response.status = MovedPermanently;
						response["Location"] = "/index.html";
						buffer.Send(response.toString());
						return -1;
					}
					fs::path filepath = buffer.RootPath() / reqest.request;
					if(!is_regular_file(filepath))
					{
						throw NotFoundException(u8"Datei oder Seite nicht gefunden");
					}
					auto ext = filepath.extension();
					if (ext == libext)
					{
						auto lib = dlopen(filepath.c_str());
						auto seite = (int(*)(RequestBuffer&))dlsym(lib, extsymbol);
						if (seite == nullptr)
						{
							dlclose(lib);
							throw ServerException(u8"Datei kann nicht ausgeführt werden");
						}
						int count = seite(buffer);
						//dlclose(lib);
						return count;
					}
					//else if (ext == ".php")
					//{
					//	//buffer.Redirect("/usr/bin/php", filepath.u8string());

					//}
					else
					{
						time_t date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
						strftime(buf.Data(), buf.Length(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
						if (reqest.Exists("If-Modified-Since") && reqest["If-Modified-Since"].toString() == buf.Data())
						{
							response.status = NotModified;
							buffer.Send(response.toString());
							return 0;
						}
						response["Last-Modified"] = buf.Data();
						std::ifstream file = std::ifstream(filepath, std::ios::binary);
						if (!file.is_open())
						{
							throw ServerException(u8"Datei kann nicht geöffnet werden");
						}
						uintmax_t filesize = fs::file_size(filepath), a = 0, b = filesize - 1;
						if (reqest.Exists("Range"))
						{
							sscanf(reqest["Range"]["bytes"].data(), "%ju-%ju", &a, &b);
							if (filesize > (b - a))
							{
								response.status = PartialContent;
								snprintf(buf.Data(), buf.Length(), "%ju-%ju/%ju", a, b, filesize);
								response["Content-Range"]["bytes"] = buf.Data();
							}
							else
							{
								response.status = RangeNotSatisfiable;
								buffer.Send(response.toString());
								return 0;
							}
						}
						else
						{
							response.status = Ok;
						}
						if (filepath.extension() == "html")
						{
							response["Content-Type"] = "text/html; charset=utf-8";
						}
						time(&date);
						strftime(buf.Data(), buf.Length(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
						response["Date"] = buf.Data();
						snprintf(buf.Data(), buf.Length(), "%ju", b - a + 1);
						response["Content-Length"] = buf.Data();
						if (reqest.putValues.toString() == "herunterladen") response["Content-Disposition"] = "attachment";
						response["Accept-Ranges"] = "bytes";
						buffer.Send(response.toString());
						if (reqest.methode != Head)
						{
							buffer.Send(file, a, b);
						}
					}
				}
			}
		}
		catch (const NotFoundException &ex)
		{
			response.status = NotFound;
			std::string errorpage("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><title>Nicht gefunden</title></head><body><div><h1>Nicht gefunden: " + std::move(std::string(ex.what())) + "</h1></div></body></html>");
			snprintf(buf.Data(), buf.Length(), "%zu", errorpage.length());
			response["Content-Length"] = buf.Data();
			buffer.Send(response.toString());
			buffer.Send(errorpage);
			std::cout << "Nicht gefunden: " << ex.what() << "\n";
		}
		catch (const std::exception& ex)
		{
			response.status = InternalServerError;
			std::string errorpage("<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><title>Interner Server Fehler</title></head><body><div><h1>Interner Server Fehler: " + std::move(std::string(ex.what())) + "</h1></div></body></html>");
			snprintf(buf.Data(), buf.Length(), "%zu", errorpage.length());
			response["Content-Length"] = buf.Data();
			buffer.Send(response.toString());
			buffer.Send(errorpage);
			std::cout << "Internal Server Error: " << ex.what() << "\n";
		}
		return 0;
	});
	std::cout << buffer->Client() << " Getrennt\n";
}