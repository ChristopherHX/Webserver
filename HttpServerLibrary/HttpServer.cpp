#include "HttpServer.h"

#include "ByteArray.h"
#include "RecvBuffer.h"
#include "HttpHeader.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#undef max
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/signal.h>
#include <dlfcn.h>
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

using namespace HttpServer;

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

bool CreateServerSocket(uintptr_t &httpServerSocket, int port)
{
	httpServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(httpServerSocket == -1)
	{
		std::cerr << "Kann den Socket nicht erstellen!";
		return false;
	}
	sockaddr_in serverAdresse;
	serverAdresse.sin_family = AF_INET;
	serverAdresse.sin_addr.s_addr = INADDR_ANY;
	serverAdresse.sin_port = htons(port);

	if (bind(httpServerSocket, (sockaddr *)&serverAdresse, sizeof(serverAdresse)) == -1)
	{
		std::cerr << "Kann den Socket nicht Binden!\n";
		return false;
	}
	if (listen(httpServerSocket, 10) == -1)
	{
		std::cerr << "Kann den Socket nicht Abhören!\n";
		return false;
	}
	return true;
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
		"./"
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

		if (SSL_CTX_use_certificate_chain_file((SSL_CTX*)ctx, publicchain.c_str()) < 0) {
			ERR_print_errors_fp(stderr);
			return;
		}

		if (SSL_CTX_use_PrivateKey_file((SSL_CTX*)ctx, privatekey.c_str(), SSL_FILETYPE_PEM) < 0) {
			ERR_print_errors_fp(stderr);
			return;
		}
		if(!CreateServerSocket(httpsServerSocket, httpsPort > 0 ? httpsPort : 433))
		{
			return;
		}
	}
	else
	{
		std::cerr << "Zertifikat(e) nicht gefunden\n";
		return;
	}

	if(!CreateServerSocket(httpServerSocket, httpPort > 0 ? httpPort : 80))
	{
		return;
	}
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
			if((httpServerSocket != -1 && FD_ISSET(httpServerSocket, &sockets)) || (httpsServerSocket != -1 && FD_ISSET(httpsServerSocket, &sockets)))
			{
				uintptr_t clientSocket = accept(FD_ISSET(httpServerSocket, &sockets) ? httpServerSocket : httpsServerSocket, (sockaddr *)&clientAdresse, &clientAdresse_len);
				if (clientSocket != -1)
				{
					SSL *ssl = nullptr;
					if(FD_ISSET(httpsServerSocket, &sockets))
					{
						ssl = SSL_new(ctx);
						SSL_set_fd(ssl, clientSocket);
						SSL_accept(ssl);
					}
					ByteArray client(clientAdresse_len + 13);
					snprintf(client.Data(), client.Length(), "http%s://%s:%hu", ssl == nullptr ? "" : "s", inet_ntop(AF_INET, &clientAdresse.sin_addr, ByteArray(clientAdresse_len).Data(), clientAdresse_len), ntohs(clientAdresse.sin_port));
					std::thread(&Server::processRequest, this, std::unique_ptr<RecvBuffer>(new RecvBuffer(clientSocket, 1 << 14, std::string(client.Data()), ssl))).detach();
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
	if(httpServerSocket != -1)
	{
#ifdef _WIN32
		shutdown(httpServerSocket, SD_BOTH);
		closesocket(httpServerSocket);
#else
		shutdown(httpServerSocket, SHUT_RDWR);
		close(httpServerSocket);
#endif
	}
	if(httpsServerSocket != -1)
	{
#ifdef _WIN32
		shutdown(httpsServerSocket, SD_BOTH);
		closesocket(httpsServerSocket);
#else
		shutdown(httpsServerSocket, SHUT_RDWR);
		close(httpsServerSocket);
#endif
	}
	((std::thread*)servermain)->join();
#ifdef _WIN32
	WSACleanup();
#endif
	delete (std::thread*)servermain;
}

void HttpServer::Server::ShowFolder(bool show)
{
	showFolder = show;
}

void HttpServer::Server::SetRootFolder(std::string path)
{
	rootfolder = path;
}

struct FileUploadArgs
{
	std::string contentSeperator;
	std::ofstream fileStream;
	int proccessedContent;
	int ContentLength;
	fs::path serverPath;
};

bool compare_nocase (const std::string& first, const std::string& second)
{
  unsigned int i=0;
  while ( (i<first.length()) && (i<second.length()) )
  {
    if (tolower(first[i])<tolower(second[i])) return true;
    else if (tolower(first[i])>tolower(second[i])) return false;
    ++i;
  }
  return ( first.length() < second.length() );
}

void Server::processRequest(std::unique_ptr<RecvBuffer> buffer)
{
	using namespace std::chrono_literals;
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif // _WIN32
	RequestArgs status;
	std::map<std::string, std::string> benutzer;
	std::cout << buffer->Client() << " Verbunden\n";
	buffer->RecvData([&status, &benutzer, this](RecvBuffer * buffer, DualByteArray array) -> int
	{
		try
		{
			int l, header = array.IndexOf("\r\n\r\n", 0, l);
			if (header > 0 && l == 4)
			{
				ResponseHeader responseHeader;
				responseHeader["Content-Length"] = "0";
				ByteArray range(array.GetRange(0, header + l));
				RequestHeader requestHeader(range.Data(), range.Length());
				status.path = buffer->Client() + " -> " + requestHeader.httpMethode + ":" + requestHeader.requestPath;
				if (Request) Request(&status);
				if (!buffer->isSecure())
				{
					responseHeader.status = MovedPermanently;
					responseHeader["Connection"] = "close";
					responseHeader["Location"] = "https://" + (requestHeader.Exists("Host") ? requestHeader["Host"] : "p4fdf5699.dip0.t-ipconnect.de") + requestHeader.requestPath;
					buffer->Send(responseHeader.toString());
					buffer->RecvDataState(false);
					return 0;
				}
				else
				{
					ByteArray buf(128);
					responseHeader["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains";
					responseHeader["Connection"] = "keep-alive";
					if (requestHeader.httpMethode == "GET" || requestHeader.httpMethode == "HEAD")
					{
						try
						{
							if (!showFolder && requestHeader.requestPath == "/")
							{
								responseHeader.status = MovedPermanently;
								responseHeader["Location"] = "/index.html";
								buffer->Send(responseHeader.toString());
								buffer->RecvDataState(false);
								return 0;
							}
							else if (requestHeader.requestPath == "/folderview.html")
							{
								std::string serverPath(ParameterValues(String::Replace(std::move(requestHeader.putPath), "%2F", "/"))["folder"]);							
								fs::path folderPath = rootfolder / serverPath;
								if(fs::is_directory(folderPath))
								{
									std::list<std::string> folder;
									std::list<std::string> files;
									for (fs::directory_entry entry : fs::directory_iterator(folderPath))
									{
										std::string name(entry.path().filename().string());
										fs::file_status status(entry.status());
										switch (status.type())
										{
										case fs::file_type::regular:
											files.push_back(name);
											break;
										case fs::file_type::directory:
											folder.push_back(name);
											break;
										default:
											break;
										}
									}
									std::stringstream contentstream;
									contentstream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/><link rel=\"stylesheet\" href=\"/folder.css\" type=\"text/css\"/></head><body><div id=\"path\">" << serverPath << "</div>";
									if(serverPath != "/") contentstream << "<div><a class=\"button button3\" href=\"/folderview.html?folder=" << serverPath.substr(0, serverPath.find_last_of('/', serverPath.length() - 2)) << "/\" target=\"_self\">..</button></div>";
									folder.sort(compare_nocase);
									for(auto &name : folder)
									{
										contentstream << "<div><a class=\"button button3\" href=\"/folderview.html?folder=" << serverPath << name << "/\" target=\"_self\">" << name << "</button></div>";
									}
									files.sort(compare_nocase);
									for(auto &name : files)
									{
										contentstream << "<div><a class=\"button button1\" href=\"" << serverPath << name << "\" target=\"_top\">" << name << "</a><a class=\"button button2\" href=\"" << serverPath << name << "?download\"></a></div>";										
									}
									contentstream << "</body>";
									std::string content(contentstream.str());
									responseHeader.status = Ok;
									snprintf(buf.Data(), buf.Length(), "%zu", content.length());
									responseHeader["Content-Type"] = "text/html; charset=utf-8";
									responseHeader["Content-Length"] = buf.Data();
									responseHeader["Cache-Control"] = "no-store, must-revalidate";
									buffer->Send(responseHeader.toString());
									if (requestHeader.httpMethode != "HEAD")
									{
										buffer->Send(content);
									}
								}
								else
								{
									throw ServerException("Folder not exists");
								}
							}
							else if(requestHeader.requestPath == "/benutzer.cpp")
							{
								std::stringstream contentstream;
								contentstream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/></head><body>";
								sscanf(requestHeader["Cookie"].data(), "Sessionid=%s", buf.Data());
								contentstream << "<h1>Willkommen " << (benutzer.find(buf.Data()) != benutzer.end() ? benutzer[buf.Data()] : "Melde dich an!!!") <<" </h1>";
								contentstream << "</body>";
								std::string content(contentstream.str());
								responseHeader.status = Ok;
								snprintf(buf.Data(), buf.Length(), "%zu", content.length());
								responseHeader["Content-Type"] = "text/html; charset=utf-8";
								responseHeader["Content-Length"] = buf.Data();
								responseHeader["Cache-Control"] = "no-store, must-revalidate";
								buffer->Send(responseHeader.toString());
								if (requestHeader.httpMethode != "HEAD")
								{
									buffer->Send(content);
								}
							}
							else
							{
								fs::path filepath = rootfolder / requestHeader.requestPath;
								if(!is_regular_file(filepath))
								{
									throw ServerException("File not exists");								
								}
								time_t date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
								strftime(buf.Data(), buf.Length(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
								if (requestHeader.Exists("If-Modified-Since") && requestHeader["If-Modified-Since"] == buf.Data())
								{
									responseHeader.status = NotModified;
									buffer->Send(responseHeader.toString());
									return header + l;
								}
								responseHeader["Last-Modified"] = buf.Data();
								std::ifstream file = std::ifstream(filepath, std::ios::binary);
								if (file.is_open())
								{
									uintmax_t filesize = fs::file_size(filepath), a = 0, b = filesize - 1;
									if (requestHeader.Exists("Range"))
									{
										std::string & ranges(requestHeader["Range"]);
										sscanf(ranges.data(), "bytes=%ju-%ju", &a, &b);
										if (filesize > (b - a))
										{
											responseHeader.status = PartialContent;
											snprintf(buf.Data(), buf.Length(), "bytes=%ju-%ju/%ju", a, b, filesize);
											responseHeader["Content-Range"] = buf.Data();
										}
										else
										{
											responseHeader.status = RangeNotSatisfiable;
											buffer->Send(responseHeader.toString());
											return header + l;
										}
									}
									else
									{
										responseHeader.status = Ok;
									}
									if(fs::path(filepath).extension() == "html" || fs::path(filepath).extension() == "htm")
									{
										responseHeader["Content-Type"] = "text/html; charset=utf-8";
									}
									time(&date);
									strftime(buf.Data(), buf.Length(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
									responseHeader["Date"] = buf.Data();
									snprintf(buf.Data(), buf.Length(), "%ju", b - a + 1);
									responseHeader["Content-Length"] = buf.Data();
									if (requestHeader.putPath == "download") responseHeader["Content-Disposition"] = "attachment";
									responseHeader["Accept-Ranges"] = "bytes";
									buffer->Send(responseHeader.toString());
									if (requestHeader.httpMethode != "HEAD")
									{
										buffer->Send(file, a, b);
									}
								}
								else
								{
									throw ServerException("File open failed");;
								}
							}
						}
						catch(const std::exception &ex)
						{
							responseHeader.status = NotFound;
							std::string errorpage("<!DOCTYPE html><HTML><HEAD><TITLE>Not Found Error</TITLE></HEAD><BODY><div><h1>" + std::move(std::string(ex.what())) + "</h1></div></BODY></HTML>");
							snprintf(buf.Data(), buf.Length(), "%zu", errorpage.length());
							responseHeader["Content-Length"] = buf.Data();
							buffer->Send(responseHeader.toString());
							if (requestHeader.httpMethode != "HEAD")
							{
								buffer->Send(errorpage);
							}
						}
					}
					else if (requestHeader.httpMethode == "POST")
					{
						std::string &contenttype = requestHeader["Content-Type"];
						if (requestHeader.requestPath == "/summit")
						{
							responseHeader.status = Ok;
							responseHeader["Content-Length"] = "9";
							buffer->Send(responseHeader.toString());
							buffer->Send("Wilkommen");
							uintmax_t size;
							sscanf(requestHeader["Content-Length"].data(), "%ju", &size);
							return header + l + size;
						}
						else if (requestHeader.requestPath == "/uploadfiles" && memcmp(contenttype.data(), "multipart/form-data", 19) == 0)
						{
							fs::path serverPath(rootfolder / requestHeader.putPath);
							if (fs::is_directory(serverPath))
							{
								FileUploadArgs args;
								args.contentSeperator = "--" + ParameterValues(contenttype)["boundary"];
								args.proccessedContent = 0;
								args.ContentLength = std::stoull(requestHeader["Content-Length"]);
								args.serverPath = std::move(serverPath);
								buffer->RecvData([&args, &status, &responseHeader](RecvBuffer * buffer, DualByteArray array){
									if (status.Progress) status.Progress(100 * args.proccessedContent / args.ContentLength);
									int contentSeperatormatch, contentSeperatorI = array.IndexOf(args.contentSeperator, 0, contentSeperatormatch);
									if (contentSeperatormatch > 0 && contentSeperatormatch < args.contentSeperator.length())
									{
										contentSeperatorI = -1;
									}
									if (args.fileStream.is_open() && (contentSeperatorI == -1 || ((long long)contentSeperatorI - 2) > 0))
									{
										int DataEnd = contentSeperatorI == -1 ? array.Length() : (contentSeperatorI - 2);
										buffer->CopyTo(args.fileStream, DataEnd);
										args.proccessedContent += contentSeperatorI == -1 ? DataEnd : contentSeperatorI;
										return contentSeperatorI == -1 ? 0 : 2;
									}
									else if (contentSeperatorI != -1)
									{
										int contentSeperatorEndI = contentSeperatorI + args.contentSeperator.length() + 2;
										int match, fileHeaderEndI = array.IndexOf("\r\n\r\n", contentSeperatorEndI, match);
										if (args.fileStream.is_open()) args.fileStream.close();
										if (fileHeaderEndI != -1)
										{
											int proccessed = fileHeaderEndI + match;
											if (match == 4)
											{
												ByteArray range(std::move(array.GetRange(contentSeperatorEndI, fileHeaderEndI - contentSeperatorEndI)));
												std::string name = ParameterValues(Parameter(std::string(range.Data(), range.Length()))["Content-Disposition"])["name"];
												args.fileStream.open((args.serverPath / name.substr(1, name.length() - 2)), std::ios::binary);
												int proccessed = fileHeaderEndI + match;
												args.proccessedContent += proccessed;
											}
											else
											{
												responseHeader.status = Ok;
												buffer->Send(responseHeader.toString());
												if (status.Progress) status.Progress(100);
												buffer->RecvDataState(false);
												return 0;
											}
											return proccessed;
										}
									}
									return 0;
								});
								buffer->RecvDataState(true);
							}
							else
							{
								responseHeader.status = NotFound;
								buffer->Send(responseHeader.toString());
							}
						}
						else if (memcmp(contenttype.data(), "application/x-www-form-urlencoded", 33) == 0)
						{
							ByteArray range(array.GetRange(header + l, std::stoull(requestHeader["Content-Length"])));
							int m, m2, un = range.IndexOf("UserName=", 0, m);
							std::string username(range.Data(), un + m, range.IndexOf("&", un + m, m2) - (un + m));
							int up = range.IndexOf("Password=", un + m, m);
							std::string password(range.Data(), up + m, range.Length() - (up + m));
							for (int i = 0; i < 32; i++)
							{
								buf[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 62];
							}
							buf[32] = 0;
							std::string sessionid(buf.Data());
							responseHeader["Set-Cookie"] = "Sessionid=" + sessionid + "; Secure; HttpOnly";
							responseHeader.status = SeeOther;
							responseHeader["Location"] = "/benutzer.cpp";
							buffer->Send(responseHeader.toString());
							benutzer[sessionid] = username;
							return header + l + range.Length();
						}
					}
					else
					{
						responseHeader.status = NotImplemented;
						buffer->Send(responseHeader.toString());
						if (status.Progress) status.Progress(100);
					}
					return header + l;
				}
			}
		}
		catch (const std::exception& ex)
		{
			std::cout << "Error:" << ex.what() << "\n";
		}
		return 0;
	});
	std::cout << buffer->Client() << " Getrennt\n";
}
