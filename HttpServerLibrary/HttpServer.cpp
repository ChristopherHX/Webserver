#include "HttpServer.h"
#include "Array.h"
#include "HttpRequestBuffer.h"
#include "Http.h"
#include "Http2.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#undef max
//#define libext ".dll"
#define libext ".so"
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
#define libext ".so"
#define dlopen(name) dlopen(name, RTLD_NOW)
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
	{
		WSADATA wsaData;
		WSAStartup(WINSOCK_VERSION, &wsaData);
	}
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
		OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
		//OPENSSL_init_ssl(0, nullptr);
		ctx = SSL_CTX_new(TLS_server_method());
		
		if (SSL_CTX_use_certificate_chain_file(ctx, publicchain.u8string().data()) < 0) {
			int i;
			ERR_print_errors_cb([](const char *str, size_t len, void *u) -> int {
				if ((*((int*)u)) == 0)
				{
					(*((int*)u))++;
					throw std::runtime_error(std::string(str, len));
				}
				return 0;
			}, &i);
		}

		if (SSL_CTX_use_PrivateKey_file(ctx, privatekey.u8string().data(), SSL_FILETYPE_PEM) < 0) {
			int i;
			ERR_print_errors_cb([](const char *str, size_t len, void *u) -> int {
				if ((*((int*)u)) == 0)
				{
					(*((int*)u))++;
					throw std::runtime_error(std::string(str, len));
				}
				return 0;
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
		uintptr_t nfds = std::max(httpServerSocket, httpsServerSocket) + 1;
		fd_set sockets;
		timeval timeout;
		sockaddr_in clientAdresse;
		socklen_t clientAdresse_len = sizeof(clientAdresse);
		FD_ZERO(&sockets);
		while (servermainstop.load())
		{
			timeout.tv_sec = 5;
			timeout.tv_usec = 0;
			if (httpServerSocket != -1)	FD_SET(httpServerSocket, &sockets);
			if (httpsServerSocket != -1)FD_SET(httpsServerSocket, &sockets);
			select(nfds, &sockets, nullptr, nullptr, &timeout);
			bool Secure = FD_ISSET(httpsServerSocket, &sockets);
			if (Secure || FD_ISSET(httpServerSocket, &sockets))
			{
				uintptr_t clientSocket = accept(Secure ? httpsServerSocket : httpServerSocket, (sockaddr *)&clientAdresse, &clientAdresse_len);
				if (clientSocket != -1)
				{
					SSL *ssl = nullptr;
					if (Secure)
					{
						ssl = SSL_new(ctx);
						SSL_set_fd(ssl, clientSocket);
						SSL_accept(ssl);
					}
					std::vector<char> client(clientAdresse_len + 6);
					snprintf(client.data(), client.size(), "%s:%hu", inet_ntop(AF_INET, &clientAdresse.sin_addr, std::vector<char>(clientAdresse_len).data(), clientAdresse_len), ntohs(clientAdresse.sin_port));
					std::thread(&Server::processRequest, this, std::unique_ptr<RequestBuffer>(new RequestBuffer(clientSocket, 1 << 14, std::string(client.data()), ssl, rootfolder))).detach();
				}
			}
		}
		if (ctx != nullptr) SSL_CTX_free(ctx);
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
	buffer->RecvData([&status, &OnRequest = onrequest](RequestBuffer &buffer) -> long long
	{
		Response &response = buffer.response();
		response.Clear();
		response["Content-Length"] = "0";
		try
		{
			int header = buffer.indexof("\r\n\r\n");
			if (header > 0)
			{
				header += 4;
				auto &reqest = buffer.request() = Request(std::string(buffer.begin(), buffer.begin() + header));
				buffer.Free(header);
				status.path = buffer.Client() + " -> " + reqest.methode + ":" + reqest.request;
				if (OnRequest) OnRequest(status);
				if (!buffer.isSecure())
				{
					if (!reqest.Exists("Host"))
					{
						throw NotFoundError("Benutze Https/1.1!!!");
					}
					response.status = MovedPermanently;
					response["Connection"]["close"];
					response["Location"]["https://" + reqest["Host"].toString() + reqest.request];
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
						reqest.request = "/index.html";
					}
					fs::path filepath = buffer.RootPath() / reqest.request;
					if(!fs::exists(filepath))
					{
						throw NotFoundError(u8"Datei oder Seite nicht gefunden");
					}
					auto ext = filepath.extension();
					if (ext == libext)
					{
						auto lib = dlopen(filepath.c_str());
						auto seite = (int(*)(RequestBuffer&))dlsym(lib, extsymbol);
						if (seite == nullptr)
						{
							dlclose(lib);
							throw ServerError(u8"Datei kann nicht ausgeführt werden");
						}
						int count = seite(buffer);
						//dlclose(lib);
						return count;
					}
//					else if (ext == ".php")
//					{
//#ifdef _WIN32
//						throw ServerError(u8"PHP Dateien können noch nicht ausgeführt werden");
//#else
//						std::cout << "sapi starting\n";
//						sapi_startup(&phpplugin);
//						std::cout << "sapi started\n";
//						php_request_startup();
//						std::cout << "php starting\n";
//						zend_file_handle file;
//						file.type = ZEND_HANDLE_FILENAME;
//						file.filename = filepath.c_str();
//						file.free_filename = 0;
//						file.opened_path = NULL;
//						std::cout << "php executing\n";
//						php_execute_script(&file);
//						sapi_shutdown();
//						std::cout << "sapi shutdown\n";
//#endif
//					}
					else if(reqest.methode == Get || reqest.methode == Head)
					{
						response["Accept-Ranges"] = "bytes";
						{
							std::vector<char> buf(128);
							time_t date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
							strftime(buf.data(), buf.size(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
							if (reqest.Exists("If-Modified-Since") && reqest["If-Modified-Since"].toString() == buf.data())
							{
								response.status = NotModified;
								buffer.Send(response.toString());
								return 0;
							}
							response["Last-Modified"] = buf.data();
							time(&date);
							strftime(buf.data(), buf.size(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
							response["Date"] = buf.data();
						}
						uintmax_t offset = 0, length = fs::file_size(filepath);
						if (reqest.Exists("Range"))
						{
							uintmax_t lpos;
							std::istringstream iss(reqest["Range"]["bytes"]);
							iss >> offset;
							iss.ignore(1, '-');
							iss >> lpos;
							if (offset <= lpos && lpos < length)
							{
								length = lpos + 1 - offset;
								response.status = PartialContent;
								response["Content-Range"]["bytes"] = reqest["Range"]["bytes"];
							}
							else
							{
								response.status = RangeNotSatisfiable;
								buffer.Send(response.toString());
								return 0;
							}
						}
						else response.status = Ok;
						response["Content-Length"] = std::to_string(length);
						if (ext == ".html") response["Content-Type"] = "text/html; charset=utf-8";
						else if (ext == ".txt") response["Content-Type"] = "text/plain; charset=utf-8";
						else if (ext == ".css") response["Content-Type"] = "text/css; charset=utf-8";
						else if (ext == ".js") response["Content-Type"] = "text/javascript; charset=utf-8";
						else if (ext == ".png") response["Content-Type"] = "image/png";
						else if (ext == ".jpg") response["Content-Type"] = "image/jpeg";
						else if (ext == ".svg") response["Content-Type"] = "text/svg; charset=utf-8";
						else if (ext == ".pdf") response["Content-Type"] = "application/pdf";
						else if (ext == ".pdf") response["Content-Type"] = "video/mpeg";
						else if (ext == ".xml") response["Content-Type"] = "text/xml";
						else if (ext == ".tiff") response["Content-Type"] = "image/tiff";
						else if (ext == ".fif") response["Content-Type"] = "image/fif";
						else if (ext == ".ief") response["Content-Type"] = "image/ief";
						else if (ext == ".gif")	response["Content-Type"] = "image/gif";
						else response["Content-Type"] = "application/octet-stream";
						if (reqest.putValues.toString() == "herunterladen") response["Content-Disposition"] = "attachment";
						{
							std::ifstream file = std::ifstream(filepath, std::ios::binary);
							if (!file.is_open()) throw ServerError(u8"Datei kann nicht geöffnet werden");
							buffer.Send(response.toString());
							if (reqest.methode != Head)	buffer.Send(file, offset, length);
						}
					}
					else throw std::runtime_error("Anfrage kann nicht verarbeitet werden: " + reqest.methode);
				}
			}
		}
		catch (const NotFoundError &ex)
		{
			response.status = NotFound;
			response["Content-Type"] = "text/html; charset=utf-8";
			std::string errorpage("<!DOCTYPE html><html><head><title>Nicht gefunden</title></head><body><div><h1>Nicht gefunden: " + std::move(std::string(ex.what())) + "</h1></div></body></html>");
			response["Content-Length"] = std::to_string(errorpage.length());
			buffer.Send(response.toString());
			buffer.Send(errorpage);
		}
		catch (const ServerError &ex)
		{
			response.status = InternalServerError;
			response["Content-Type"] = "text/html; charset=utf-8";
			std::string errorpage("<!DOCTYPE html><html><head><title>Interner Server Fehler</title></head><body><div><h1>Interner Server Fehler: " + std::move(std::string(ex.what())) + "</h1></div></body></html>");
			response["Content-Length"] = std::to_string(errorpage.length());
			buffer.Send(response.toString());
			buffer.Send(errorpage);
		}
		catch (const std::exception& ex)
		{
			std::cout << ex.what() << "\n";
			return -1;
		}
		return 0;
	});
	std::cout << buffer->Client() << " Getrennt\n";
}