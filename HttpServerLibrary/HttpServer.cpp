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
#include <experimental/filesystem>
#include <iostream>
#include <cstdio>

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

void Server::Starten(const int port, const int sslport)
{
	std::string certroot =
#ifdef _WIN32
		"./"
#else
		"/etc/letsencrypt/live/p4fdf5699.dip0.t-ipconnect.de/"
#endif
		;
	std::string publicchain = certroot + "fullchain.pem";
	std::string privatekey = certroot + "privkey.pem";
	serverSocket = -1;
	sslServerSocket = -1;
	if (servermainstop.load())
		return;
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(WINSOCK_VERSION, &wsaData);
#endif
	SSL_CTX* ctx = nullptr;

	if(sslport > 0)
	{
		if (fs::is_regular_file(publicchain) && fs::is_regular_file(privatekey))
		{
			SSL_load_error_strings();
			OpenSSL_add_ssl_algorithms();

			ctx = SSL_CTX_new(TLS_server_method());

			if (SSL_CTX_use_certificate_chain_file((SSL_CTX*)ctx, publicchain.data()) < 0) {
				ERR_print_errors_fp(stderr);
				exit(EXIT_FAILURE);
			}

			if (SSL_CTX_use_PrivateKey_file((SSL_CTX*)ctx, privatekey.data(), SSL_FILETYPE_PEM) < 0) {
				ERR_print_errors_fp(stderr);
				exit(EXIT_FAILURE);
			}

			sslServerSocket = socket(AF_INET, SOCK_STREAM, 0);
			if(sslServerSocket == -1)
			{
				std::cout << "Kann den SSL Socket nicht erstellen!";
			}
			sockaddr_in secserverAdresse;
			secserverAdresse.sin_family = AF_INET;
			secserverAdresse.sin_addr.s_addr = INADDR_ANY;
			secserverAdresse.sin_port = htons(sslport);
			if (bind(sslServerSocket, (sockaddr *)&secserverAdresse, sizeof(secserverAdresse)) == -1)
			{
				std::cerr << "Kann den SSL Socket nicht Binden!";
			}
			if (listen(sslServerSocket, 10) == -1)
			{
				std::cerr << "Kann den SSL Socket nicht Abhören!";
			}
		}
		else
		{
			std::cerr << "Achtung Zertifikat(e) nicht gefunden!\n";
		}
	}

	if(port > 0)
	{
		serverSocket = socket(AF_INET, SOCK_STREAM, 0);
		if(serverSocket == -1)
		{
			std::cerr << "Kann den Socket nicht erstellen!";
		}
		sockaddr_in serverAdresse;
		serverAdresse.sin_family = AF_INET;
		serverAdresse.sin_addr.s_addr = INADDR_ANY;
		serverAdresse.sin_port = htons(port);

		if (bind(serverSocket, (sockaddr *)&serverAdresse, sizeof(serverAdresse)) == -1)
		{
			std::cerr << "Kann den Socket nicht Binden!\n";
		}
		if (listen(serverSocket, 10) == -1)
		{
			std::cerr << "Kann den Socket nicht Abhören!\n";
		}
	}
	servermainstop.store(true);
	servermain = (void*)new std::thread([this, ctx]() {
		int nfds = std::max(serverSocket != -1 ? serverSocket : 0, sslServerSocket != -1 ? sslServerSocket : 0) + 1;
		fd_set sockets;
		timeval timeout;
		sockaddr_in clientAdresse;
		socklen_t clientAdresse_len = sizeof(clientAdresse);
		FD_ZERO(&sockets);
		while (servermainstop.load())
		{
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			if (serverSocket != -1)
			{
				FD_SET(serverSocket, &sockets);
			}
			if (sslServerSocket != -1)
			{
				FD_SET(sslServerSocket, &sockets);
			}
			select(nfds, &sockets, nullptr, nullptr, &timeout);
			if((serverSocket != -1 && FD_ISSET(serverSocket, &sockets)) || (sslServerSocket != -1 && FD_ISSET(sslServerSocket, &sockets)))
			{
				uintptr_t clientSocket = accept(FD_ISSET(serverSocket, &sockets) ? serverSocket : sslServerSocket, (sockaddr *)&clientAdresse, &clientAdresse_len);
				if (clientSocket != -1)
				{
					SSL *ssl = nullptr;
					if(FD_ISSET(sslServerSocket, &sockets))
					{
						ssl = SSL_new(ctx);
						SSL_set_fd(ssl, clientSocket);
						SSL_accept(ssl);
					}
					ByteArray buf(clientAdresse_len + 13);
					snprintf(buf.Data(), buf.Length(), "http%s://%s:%hu", ssl == nullptr ? "" : "s", inet_ntop(AF_INET, &clientAdresse.sin_addr, ByteArray(clientAdresse_len).Data(), clientAdresse_len), ntohs(clientAdresse.sin_port));
					std::thread(&Server::processRequest, this, std::unique_ptr<RecvBuffer>(new RecvBuffer(clientSocket, 1 << 14, std::string(buf.Data()), ssl))).detach();
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
	if(serverSocket != -1)
	{
#ifdef _WIN32
		shutdown(serverSocket, SD_BOTH);
		closesocket(serverSocket);
#else
		shutdown(serverSocket, SHUT_RDWR);
		close(serverSocket);
#endif
	}
	if(sslServerSocket != -1)
	{
#ifdef _WIN32
		shutdown(sslServerSocket, SD_BOTH);
		closesocket(sslServerSocket);
#else
		shutdown(sslServerSocket, SHUT_RDWR);
		close(sslServerSocket);
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
	std::string filepath;
};

void Server::processRequest(std::unique_ptr<RecvBuffer> pbuffer)
{
	using namespace std::chrono_literals;
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
#endif // _WIN32
	void * args = nullptr;
	RequestArgs status;
	ResponseHeader responseHeader;
	RequestState state = HandleRequest;
	std::cout << pbuffer->GetIP() << " Verbunden\n";
	pbuffer->RecvData([&state, &responseHeader, &status, &args, this](RecvBuffer * buffer, DualByteArray array) -> int
	{
		try
		{
			switch (state)
			{
			case HandleRequest:
			{
				int l, header = array.IndexOf("\r\n\r\n", 0, l);
				if (header > 0 && l == 4)
				{
					ByteArray range(array.GetRange(0, header + 4));
					RequestHeader requestHeader(range.Data(), range.Length());
					status.path = buffer->GetIP() + " -> " + requestHeader.httpMethode + ":" + requestHeader.requestPath;
					if (Request) Request(&status);
					std::string filepath = rootfolder + requestHeader.requestPath;
					if (!buffer->isSecure())
					{
						responseHeader.status = MovedPermanently;
						responseHeader["Connection"] = "close";
						responseHeader["Location"] = "https://" + (requestHeader.Exists("Host") ? requestHeader["Host"] : "p4fdf5699.dip0.t-ipconnect.de") + requestHeader.requestPath;
						buffer->Send(responseHeader.toString());
						buffer->StopRecvData();
					}
					else
					{
						ByteArray buf(128);
						time_t date;
						time(&date);
						tm *datetime = std::gmtime(&date);
						datetime->tm_year++;
						strftime(buf.Data(), buf.Length(), "%a, %d-%b-%G %H:%M:%S GMT", datetime);
						responseHeader["Strict-Transport-Security"] = "max-age=" + std::string(buf.Data());
						responseHeader["Connection"] = "keep-alive";
						if (requestHeader.httpMethode == "GET" || requestHeader.httpMethode == "HEAD")
						{
							if (!showFolder && requestHeader.requestPath == "/")
							{
								responseHeader.status = MovedPermanently;
								responseHeader["Location"] = "/index.html";
								buffer->Send(responseHeader.toString());
								buffer->StopRecvData();				
								break;
							}
							else if (requestHeader.requestPath == "/folderview.html")
							{
								std::stringstream contentstream;
								std::string folderpath(ParameterValues(String::Replace(std::move(requestHeader.putPath), "%2F", "/"))["folder"]);
								contentstream << "<!DOCTYPE html><html><head><link rel=\"stylesheet\" href=\"/folder.css\" type=\"text/css\"/></head><body><div id=\"path\">" << folderpath << "</div>";
								if(folderpath != "/") contentstream << "<div><a class=\"button button3\" href=\"/folderview.html?folder=" << folderpath.substr(0, folderpath.find_last_of('/', folderpath.length() - 2)) << "/\" target=\"_self\">..</button></div>";
								for (fs::directory_entry entry : fs::directory_iterator(rootfolder + folderpath))
								{
									std::string name(entry.path().filename().string());
									fs::file_status status(entry.status());
									switch (status.type())
									{
									case fs::file_type::regular:
										contentstream << "<div><a class=\"button button1\" href=\"" << folderpath << name << "\" target=\"_top\">" << name << "</a><a class=\"button button2\" href=\"" << folderpath << name << "?download\"></a></div>";
										break;
									case fs::file_type::directory:
										contentstream << "<div><a class=\"button button3\" href=\"/folderview.html?folder=" << folderpath << name << "/\" target=\"_self\">" << name << "</button></div>";
										break;
									default:
										break;
									}
								}
								contentstream << "</body>";
								std::string content(contentstream.str());
								responseHeader.status = Ok;
								snprintf(buf.Data(), buf.Length(), "%zu", content.length());
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
								fs::file_status fstate = fs::status(filepath);
								switch (fstate.type())
								{
								case fs::file_type::regular:
								{
									date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
									strftime(buf.Data(), buf.Length(), "%a, %d-%b-%G %H:%M:%S GMT", std::gmtime(&date));
									if (requestHeader.Exists("If-Modified-Since") && requestHeader["If-Modified-Since"] == buf.Data())
									{
										responseHeader.status = NotModified;
										buffer->Send(responseHeader.toString());
										break;
									}
									responseHeader["Last-Modified"] = buf.Data();
									std::ifstream file = std::ifstream(filepath.data(), std::ios::binary);
									if (file.is_open())
									{
										uintmax_t filesize = fs::file_size(filepath), a = 0, b = filesize - 1;
										if (requestHeader.Exists("Range"))
										{
											std::string & ranges(requestHeader["Range"]);
											sscanf(ranges.data(), "bytes=%ju-%ju", &a, &b);
											if (filesize > (b - a) && (intmax_t)(b - a) >= 0)
											{
												responseHeader.status = PartialContent;
												snprintf(buf.Data(), buf.Length(), "bytes=%ju-%ju/%ju", a, b, filesize);
												responseHeader["Content-Range"] = buf.Data();
											}
											else
											{
												responseHeader.status = RangeNotSatisfiable;
												responseHeader["Content-Length"] = "0";
												buffer->Send(responseHeader.toString());
												break;
											}
										}
										else
										{
											responseHeader.status = Ok;
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
										std::cerr << "File can't opened:" << filepath << "\n";
									}
									break;
								}
								default:
								{
									responseHeader.status = NotFound;
									std::string errorpage("<!DOCTYPE html><HTML><HEAD><TITLE>Not Found</TITLE></HEAD><BODY><div><a>404 Not Found</a></div></BODY></HTML>");
									snprintf(buf.Data(), buf.Length(), "%zu", errorpage.length());
									responseHeader["Content-Length"] = buf.Data();
									buffer->Send(responseHeader.toString());
									if (requestHeader.httpMethode != "HEAD")
									{
										buffer->Send(errorpage);
									}
									break;
								}
								}
							}
							if (status.Progress) status.Progress(100);
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
								std::string folderpath(rootfolder + requestHeader.putPath);
								if (fs::is_directory(folderpath))
								{
									args = (void*)new FileUploadArgs();
									((FileUploadArgs*)args)->contentSeperator = "--" + ParameterValues(contenttype)["boundary"];
									((FileUploadArgs*)args)->proccessedContent = 0;
									((FileUploadArgs*)args)->ContentLength = std::stoull(requestHeader["Content-Length"]);
									((FileUploadArgs*)args)->filepath = std::move(folderpath);
									state = FileUpload;
								}
								else
								{
									responseHeader.status = NotFound;
									responseHeader["Connection"] = "keep-alive";
									responseHeader["Content-Length"] = "0";
									buffer->Send(responseHeader.toString());
								}
							}
							else if (memcmp(contenttype.data(), "application/x-www-form-urlencoded", 33) == 0)
							{
								ByteArray range(array.GetRange(header + 4, std::stoull(requestHeader["Content-Length"])));
								int m, un = range.IndexOf("UserName=", 0, m);
								std::string username(range.Data(), un + m, range.IndexOf("&", un + m, m) - (un + m));
								int up = range.IndexOf("Password=", un + m, m);
								std::string password(range.Data(), up + m, range.Length() - (up + m));
								ByteArray buf(33);
								for (int i = 0; i < 32; i++)
								{
									buf[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 62];
								}
								buf[32] = 0;
								std::string sessionid(buf.Data());
								responseHeader["Set-Cookie"] = "Sessionid=" + sessionid + "; Secure; HttpOnly";
								responseHeader.status = SeeOther;
								responseHeader["Location"] = requestHeader["Referer"].empty() ? "/" : requestHeader["Referer"];
								buffer->Send(responseHeader.toString());
								if (status.Progress) status.Progress(100);
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
				break;
			}
			case FileUpload:
			{
				if (status.Progress) status.Progress(100 * ((FileUploadArgs*)args)->proccessedContent / ((FileUploadArgs*)args)->ContentLength);
				int contentSeperatormatch, contentSeperatorI = array.IndexOf(((FileUploadArgs*)args)->contentSeperator, 0, contentSeperatormatch);
				if (contentSeperatormatch > 0 && contentSeperatormatch < ((FileUploadArgs*)args)->contentSeperator.length())
				{
					contentSeperatorI = -1;
				}
				if (((FileUploadArgs*)args)->fileStream.is_open() && (contentSeperatorI == -1 || ((long long)contentSeperatorI - 2) > 0))
				{
					int DataEnd = contentSeperatorI == -1 ? array.Length() : (contentSeperatorI - 2);
					buffer->CopyTo(((FileUploadArgs*)args)->fileStream, DataEnd);
					((FileUploadArgs*)args)->proccessedContent += contentSeperatorI == -1 ? DataEnd : contentSeperatorI;
					return contentSeperatorI == -1 ? 0 : 2;
				}
				else if (contentSeperatorI != -1)
				{
					int contentSeperatorEndI = contentSeperatorI + ((FileUploadArgs*)args)->contentSeperator.length() + 2;
					int match, fileHeaderEndI = array.IndexOf("\r\n\r\n", contentSeperatorEndI, match);
					if (((FileUploadArgs*)args)->fileStream.is_open()) ((FileUploadArgs*)args)->fileStream.close();
					if (fileHeaderEndI != -1)
					{
						int proccessed = fileHeaderEndI + match;
						if (match == 4)
						{
							ByteArray range(std::move(array.GetRange(contentSeperatorEndI, fileHeaderEndI - contentSeperatorEndI)));
							std::string name = ParameterValues(Parameter(std::string(range.Data(), range.Length()))["Content-Disposition"])["name"];
							((FileUploadArgs*)args)->fileStream.open((((FileUploadArgs*)args)->filepath + "/" + name.substr(1, name.length() - 2)).data(), std::ios::binary);
							int proccessed = fileHeaderEndI + match;
							((FileUploadArgs*)args)->proccessedContent += proccessed;
						}
						else
						{
							responseHeader.status = Ok;
							responseHeader["Connection"] = "keep-alive";
							responseHeader["Content-Length"] = "0";
							buffer->Send(responseHeader.toString());
							if (status.Progress) status.Progress(100);
							state = HandleRequest;
							delete (FileUploadArgs*)args;
							args = nullptr;
						}
						return proccessed;
					}
				}
				break;
			}
			}
		}
		catch (const std::exception& exc)
		{
			std::cout << "Error:" << exc.what() << "\n";
		}
		return 0;
	});
	std::cout << pbuffer->GetIP() << " Getrennt\n";
}
