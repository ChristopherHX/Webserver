#include "Http2.h"
#include "Http.h"
#include "hpack.h"
#include "utility.h"

#include <thread>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <atomic>
#include <openssl/ssl.h>
#include <openssl/err.h>

#ifdef _WIN32
#define dlopen(name) LoadLibraryExW(name, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)
#define dlsym(lib, symbol) GetProcAddress((HMODULE)lib, symbol)
#define dlclose(lib) FreeLibrary((HMODULE)lib)
#define initsymbol "?init@@YAXXZ"
#define mainsymbol "?requesthandler@@YAXAEAUConnection@Http2@@AEAUStream@2@AEBVpath@v1@filesystem@experimental@std@@AEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@8@3@Z"
#define deinitsymbol "?deinit@@YAXXZ"
#else
#define dlopen(name) dlopen(name,0);
#define initsymbol "?"
#define mainsymbol "?"
#define deinitsymbol "?"
#endif

using namespace Http2;
using namespace Http2::HPack;
using namespace Utility;
using namespace std::chrono_literals;
namespace fs = std::experimental::filesystem;

std::pair<fs::path, std::string> Http2::MimeTypeTable[] = {
	{ ".html" , "text/html; charset=utf-8" },
	{ ".css" , "text/css; charset=utf-8" },
	{ ".js", "text/javascript; charset=utf-8" },
	{ ".xml" , "text/xml; charset=utf-8" },
	{ ".json" , "text/json; charset=utf-8" },
	{ ".svg", "text/svg; charset=utf-8" },
	{ ".txt" , "text/plain; charset=utf-8" },
	{ ".png" , "image/png" },
	{ ".jpg" , "image/jpeg" },
	{ ".tiff" , "image/tiff" },
	{ ".fif", "image/fif" },
	{ ".ief" , "image/ief" },
	{ ".gif", "image/gif" },
	{ ".pdf", "application/pdf" },
	{ ".mpg", "video/mpeg"},
	{"", "application/octet-stream" }
};

bool StreamSearch(const Stream & stream, uint32_t streamIndentifier)
{
	return stream.streamIndentifier == streamIndentifier;
}

Http2::Server::Server(const std::experimental::filesystem::path & certroot, const std::experimental::filesystem::path & rootpath) : conhandler(/*std::thread::hardware_concurrency()*/1), running(true), connections(20), rootpath(rootpath), libs(10)
{
	connectionsend = connectionsbegin = connections.begin();
	libsend = libs.begin();
#ifdef _WIN32
	{
		WSADATA wsaData;
		WSAStartup(WINSOCK_VERSION, &wsaData);
	}
#endif
	{
		fs::path publicchain = certroot / "fullchain.pem";
		fs::path privatekey = certroot / "privkey.pem";
		if (!fs::is_regular_file(publicchain) || !fs::is_regular_file(privatekey))
		{
			throw std::runtime_error("Zertifikat(e) nicht gefunden\nServer kann nicht gestartet werden");
		}
		OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
		sslctx = SSL_CTX_new(TLS_server_method());
		if (SSL_CTX_use_certificate_chain_file(sslctx, publicchain.u8string().data()) < 0) {
			throw std::runtime_error(u8"Der Öffentlicher Schlüssel konnte nicht gelesen werden");
		}
		if (SSL_CTX_use_PrivateKey_file(sslctx, privatekey.u8string().data(), SSL_FILETYPE_PEM) < 0) {
			throw std::runtime_error(u8"Der Privater Schlüssel konnte nicht gelesen werden");
		}
	}
	SSL_CTX_set_alpn_select_cb(sslctx, [](SSL * ssl, const unsigned char ** out, unsigned char * outlen, const unsigned char * in, unsigned int inlen, void * args) -> int
	{
		return SSL_select_next_proto((unsigned char **)out, outlen, (const unsigned char *)"\x2h2", 3, in, inlen) == OPENSSL_NPN_NEGOTIATED ? 0 : 1;
	}, nullptr);
	ssocket = socket(AF_INET6, SOCK_STREAM, 0);
	if (ssocket == -1)
	{
		throw std::runtime_error(u8"Der Server Socket konnte nicht erstellt werden");
	}
	{
		uint32_t val = 0;
		if (setsockopt(ssocket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&val, sizeof(val)) != 0)
		{
			throw std::runtime_error(u8"IPV6_V6ONLY konnte nicht deaktiviert werden");
		}
	}
	{
		sockaddr_in6 serverAdresse;
		memset(&serverAdresse, 0, sizeof(sockaddr_in6));
		serverAdresse.sin6_family = AF_INET6;
		serverAdresse.sin6_addr = in6addr_any;
		serverAdresse.sin6_port = htons(443);
		if (bind(ssocket, (sockaddr *)&serverAdresse, sizeof(serverAdresse)) == -1)
		{
			throw std::runtime_error(u8"Der Server Socket kann nicht gebunden werden");
		}
	}
	if (listen(ssocket, 10) == -1)
	{
		throw std::runtime_error(u8"Der Server Socket kann nicht gelauscht werden");
	}
	FD_ZERO(&sockets);
	FD_SET(ssocket, &sockets);
	for (int i = 0; i < conhandler.length(); ++i)
	{
		conhandler[i] = std::thread(&Server::connectionshandler, this);
	}
}

Server::~Server()
{
	running = false;
	for (int i = 0; i < conhandler.length(); ++i)
	{
		if (conhandler[i].joinable())
		{
			conhandler[i].join();
		}
	}
	for (auto conit = connectionsbegin, end = connectionsend; conit != end; ++conit)
	{
		Connection & con = *conit;
		if (con.csocket != -1 && con.rmtx.try_lock())
		{
			SSL_free(con.cssl);
			Http::CloseSocket(con.csocket);
			con.rmtx.unlock();
		}
	}
	connectionsbegin = connectionsend;
	for (auto lib = libs.begin(); lib != libsend; ++lib)
	{
		void* handle = std::get<1>(*lib);
		if (handle != 0)
		{
			auto deinit = (void(*)())dlsym(handle, deinitsymbol);
			if (deinit != nullptr) deinit();
			dlclose(handle);
		}
	}
	libsend = libs.begin();
}

bool ValidFile(fs::path &filepath, std::string &uri)
{
	if (fs::is_regular_file(filepath))
	{
		return true;
	}
	else
	{
		fs::path pathbuilder;
		for(auto pos = filepath.begin(), end = filepath.end(); pos != end; ++pos)
		{
			if (fs::is_regular_file(pathbuilder /= *pos))
			{
				uri = "";
				while (++pos != end)
				{
					uri += "/" + pos->u8string();
				}
				filepath = pathbuilder;
				return true;
			}
		}
		return false;
	}
}

void Server::connectionshandler()
{
	timeval timeout = { 0,0 }, poll = { 0,0 };
	size_t nfds = ssocket + 1;
	while (running)
	{
		{
			fd_set tmp = sockets;
			select(nfds, &tmp, nullptr, nullptr, &timeout);
			active = tmp;
		}
		if (FD_ISSET(ssocket, &active) && connectionsend->rmtx.try_lock())
		{
			Connection & con = *connectionsend;
			socklen_t sockaddr_len = sizeof(sockaddr_in6);
			con.csocket = accept(ssocket, (sockaddr *)&con.adresse, &sockaddr_len);
			if (con.csocket == -1)
			{
				con.rmtx.unlock();
				continue;
			}
			con.cssl = SSL_new(sslctx);
			SSL_set_fd(con.cssl, con.csocket);
			if (SSL_accept(con.cssl) != 1)
			{
				SSL_free(con.cssl);
				Http::CloseSocket(con.csocket);
				con.cssl = 0;
				con.rmtx.unlock();
				continue;
			}
			{
				char str[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, &(con.adresse.sin6_addr), str, INET6_ADDRSTRLEN);
				std::cout << str << ":" << ntohs(con.adresse.sin6_port) << " Verbunden\n";
			}
			con.pending = 0;
			con.rbuf = Array<uint8_t>(1024);
			con.routput = con.rinput = con.rbuf.begin();
			con.wbuf = Array<uint8_t>(1024);
			con.woutput = con.winput = con.wbuf.begin();
			con.streams = Array<Stream>(100);
			con.streamsend = con.streams.begin();
			FD_SET(con.csocket, &sockets);
			nfds = std::max(con.csocket + 1, nfds);
			++connectionsend;
			con.rmtx.unlock();
			timeout = { 0,0 };
		}
		else
		{
			timeout = { 0,1000 };
			for (auto conit = connectionsbegin, end = connectionsend; conit != end; ++conit)
			{
				int64_t length;
				Connection & con = *conit;
				if (con.cssl != nullptr && (length = con.rinput.PointerWriteableLength(con.routput)) > 0 && con.rmtx.try_lock())
				{
					if (((con.pending > 0) || (select(nfds, &active, nullptr, nullptr, &poll) && FD_ISSET(con.csocket, &active))))
					{
						if ((length = SSL_read(con.cssl, con.rinput.Pointer(), length)) <= 0)
						{
							std::cout << "Verbindung verloren:" << length << "\n";
							con.wmtx.lock();
							if (con.csocket != -1)
							{
								SSL_free(con.cssl);
								FD_CLR(con.csocket, &sockets);
								Http::CloseSocket(con.csocket);
								con.cssl = 0;
							}
							for (; connectionsbegin != connectionsend && connectionsbegin->csocket == -1; ++connectionsbegin);
							con.pending = 0;
							con.wmtx.unlock();
							continue;
						}
						else
						{
							std::cout << "Empfangen:" << length << "\n";

							//FD_CLR(con.csocket, &active);//can evntl crash __i=155 ???
							con.pending = SSL_pending(con.cssl);
							con.rinput += length;
						}
						timeout = { 0,0 };
					}
					con.rmtx.unlock();
				}
				if (con.cssl != nullptr && (length = con.rinput - con.routput) >= 9 && con.wmtx.try_lock())
				{
					std::cout << "Verarbeiten:" << length << "\n";
					const char http2preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
					if (std::search(con.routput, con.rinput, http2preface, http2preface + 24) == con.routput)
					{
						con.routput += 24;
						std::cout << http2preface;
						if ((length = con.rinput - con.routput) < 9)
						{
							con.wmtx.unlock();
							continue;
						}
					}
					Frame frame;
					*std::reverse_copy(con.routput, con.routput + 3, (unsigned char*)&frame.length) = 0;
					con.routput += 3;
					frame.type = (Frame::Type)*con.routput++;
					frame.flags = (Frame::Flags)*con.routput++;
					std::reverse_copy(con.routput, con.routput + 4, (unsigned char*)&frame.streamIndentifier);
					con.routput += 4;
					if (frame.type > Frame::Type::CONTINUATION)
					{
						con.wmtx.unlock();
						continue;
					}
					{
						int i = 100;
						while ((con.rinput - con.routput) < frame.length && i >= 0)
						{
							std::this_thread::sleep_for(1ms);
							
							i--;
						}
						if (i < 0)
						{
							con.rmtx.lock();
							if (con.csocket != -1)
							{
								SSL_free(con.cssl);
								FD_CLR(con.csocket, &sockets);
								Http::CloseSocket(con.csocket);
								con.cssl = 0;
							}
							for (; connectionsbegin != connectionsend && connectionsbegin->csocket == -1; ++connectionsbegin);
							con.rmtx.unlock();
							continue;
						}
					}
					auto frameend = con.routput + frame.length;
					switch (frame.type)
					{
					case Frame::Type::DATA:
					{
						con.routput += frame.length;
						break;
					}
					case Frame::Type::HEADERS:
					{
						Stream &stream = *con.streamsend++;
						stream.state = (((uint8_t)frame.flags & (uint8_t)Frame::Flags::END_STREAM) == (uint8_t)Frame::Flags::END_STREAM) ? Stream::State::half_closed_remote : Stream::State::open;
						stream.streamIndentifier = frame.streamIndentifier;
						uint32_t hl = frame.length;
						if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::PADDED) == (uint8_t)Frame::Flags::PADDED)
						{
							uint8_t padlength = *con.routput++;
							hl--;
						}
						if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::PRIORITY) == (uint8_t)Frame::Flags::PRIORITY)
						{
							std::reverse_copy(con.routput, con.routput + 4, (unsigned char*)&stream.priority.streamDependency);
							con.routput += 4;
							stream.priority.weight = *con.routput++;
							hl -= 5;
						}
						if (frame.length < hl)
						{
							continue;
						}
						std::cout << "Create Header Array\n";
						stream.headerlist = Array<std::pair<std::string, std::string>>(20);
						stream.headerlistend = stream.headerlist.begin();
						std::cout << "Decode Header Block\n";
						con.routput = con.hdecoder.Headerblock(con.routput, con.routput + hl, stream.headerlistend);
						std::cout << "Decoded Header Block\n";
						std::string search = ":path";
						auto res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
						if (res != stream.headerlistend)
						{
							fs::path filepath = rootpath;
							std::string args, uri;
							{
								size_t pq = res->second.find('?');
								if (pq != std::string::npos)
								{
									args = Utility::urlDecode(String::Replace(res->second.substr(pq + 1), "+", " "));
									filepath /= Utility::urlDecode(res->second.substr(0, pq));
								}
								else
								{
									filepath /= Utility::urlDecode(res->second);
								}
							}
							std::cout << filepath << "\n";
							{
								Utility::Array<std::pair<std::string, std::string>> headerlist(20);
								Utility::RotateIterator<std::pair<std::string, std::string>> headerlistend = headerlist.begin();

								if (ValidFile(filepath, uri))
								{
									fs::path ext = filepath.extension();
									if (ext == ".dll")
									{
										auto res = std::search(libs.begin(), libsend, &filepath, &filepath + 1, [](const std::tuple<std::experimental::filesystem::path, void*, void(*)(Connection&, Stream&, const fs::path&, const std::string&, const std::string&)> & left, const std::experimental::filesystem::path & right) {
											return std::get<0>(left) == right;
										});
										void(*requesthandler)(Connection&, Stream&, const fs::path&, const std::string&, const std::string&);
										if (res == libsend)
										{
											libsmtx.lock();
											auto &lib = std::get<1>(*libsend);
											lib = dlopen(filepath.c_str());
											auto init = (void(*)())dlsym(lib, initsymbol);
											if (init != nullptr) init();
											requesthandler = std::get<2>(*libsend) = (void(*)(Connection&, Stream&, const fs::path&, const std::string&, const std::string&))dlsym(lib, mainsymbol);
											if (requesthandler == nullptr)
											{
												dlclose(lib);
												libsmtx.unlock();
												throw std::runtime_error(u8"Datei kann nicht ausgeführt werden");
											}
											std::get<0>(*libsend) = filepath;
											++libsend;
											libsmtx.unlock();
										}
										else
										{
											requesthandler = std::get<2>(*res);
										}
										requesthandler(con, stream, filepath, uri, args);
									}
									else
									{
										search = ":method";
										res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
										if (res != stream.headerlistend && (res->second == Http::Get || res->second == Http::Head))
										{
											{
												std::cout << res->second << "\n";
												Utility::Array<char> buf(128);
												time_t date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
												strftime(buf.data(), buf.length(), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&date));
												search = "if-modified-since";
												res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
												if (res != stream.headerlistend && res->second == buf.data())
												{
													*headerlistend++ = { ":status", "304" };
													con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
													{
														uint32_t l = (con.winput - con.woutput) - 9;
														std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
													}
													con.woutput[4] |= (unsigned char)Frame::Flags::END_STREAM;
													do
													{
														con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
													} while (con.woutput != con.winput);
													break;
												}
												*headerlistend++ = { ":status", "200" };
												*headerlistend++ = { "last-modified", buf.data() };
												time(&date);
												strftime(buf.data(), buf.length(), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&date));
												*headerlistend++ = { "date", buf.data() };
												*headerlistend++ = { "accept-ranges", "bytes" };
											}
											uintmax_t offset = 0, length = fs::file_size(filepath);
											search = "range";
											res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
											if (res != stream.headerlistend)
											{
												uintmax_t lpos = length - 1;
												std::istringstream iss(res->second.substr(6));
												iss >> offset;
												iss.ignore(1, '-');
												iss >> lpos;
												if (offset <= lpos && lpos < length)
												{
													length = lpos + 1 - offset;
													*headerlist.begin() = { ":status", "206" };
												}
												else
												{
													*headerlist.begin() = { ":status", "416" };
													con.winput += 3;
													*con.winput++ = (unsigned char)Frame::Type::HEADERS;
													*con.winput++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
													con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + sizeof(frame.streamIndentifier), con.winput);
													con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
													{
														uint32_t l = (con.winput - con.woutput) - 9;
														std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
													}
													do
													{
														con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
													} while (con.woutput != con.winput);
													break;
												}
											}
											*headerlistend++ = { "content-length", std::to_string(length) };
											{
												*headerlistend++ = { "content-type", std::search(MimeTypeTable, MimeTypeTable + (sizeof(MimeTypeTable) / sizeof(std::pair<fs::path, std::string>) - 1), &ext, &ext + 1, HPack::KeySearch<fs::path, std::string>)->second };
											}
											if (args == "herunterladen")
											{
												*headerlistend++ = { "content-disposition", "attachment" };
											}
											{
												std::ifstream stream = std::ifstream(filepath, std::ios::binary);
												if (!stream.is_open()) throw std::runtime_error(u8"Datei kann nicht geöffnet werden");
												con.winput += 3;
												*con.winput++ = (unsigned char)Frame::Type::HEADERS;
												*con.winput++ = (unsigned char)Frame::Flags::END_HEADERS;
												con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + sizeof(frame.streamIndentifier), con.winput);
												con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
												{
													uint32_t l = (con.winput - con.woutput) - 9;
													std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
												}
												do
												{
													con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
												} while (con.woutput != con.winput);
												for (long long proc = 0; proc < length;)
												{
													uint64_t count = std::min((uint64_t)(length - proc), (con.winput + 9).PointerWriteableLength(con.woutput));
													con.winput = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, con.winput);
													*con.winput++ = (unsigned char)Frame::Type::DATA;
													*con.winput++ = count == (length - proc) ? (unsigned char)Frame::Flags::END_STREAM : 0;
													con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + 4, con.winput);
													stream.read((char*)con.winput.Pointer(), count);
													con.winput += count;
													proc += count;
													do
													{
														con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
													} while (con.woutput != con.winput);
												}

												//{
												//	long long read = 0, sent = 0;
												//	std::atomic<bool> interrupt(true);
												//	stream.seekg(offset, std::ios::beg);
												//	std::thread readfileasync([streamid = &frame.streamIndentifier, &con, &stream, &length, &read, &interrupt]() {
												//		try {
												//			long long count;
												//			while (read < length && interrupt.load())
												//			{
												//				if ((count = std::min(length - read, (uint64_t)(con.winput + 9).PointerWriteableLength(con.woutput))) > 0)
												//				{
												//					con.winput = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, con.winput);
												//					*con.winput++ = (unsigned char)Frame::Type::DATA;
												//					*con.winput++ = count == (length - read) ? (unsigned char)Frame::Flags::END_STREAM : 0;
												//					con.winput = std::reverse_copy((unsigned char*)streamid, (unsigned char*)streamid + 4, con.winput);
												//					stream.read((char*)con.winput.Pointer(), count);
												//					con.winput += count;
												//					read += count;
												//				}
												//			}
												//		}
												//		catch (const std::exception &ex)
												//		{

												//		}
												//		interrupt.store(false);
												//	});
												//	long long count;
												//	while (sent < length && interrupt.load() || con.woutput != con.winput)
												//	{
												//		if ((count = con.woutput.PointerReadableLength(con.winput)) > 0)
												//		{
												//			if ((count = SSL_write((SSL*)con.cssl, con.woutput.Pointer(), count)) <= 0)
												//			{
												//				interrupt.store(false);
												//				readfileasync.join();
												//				throw std::runtime_error("RequestBuffer::Send(std::istream &stream, long long offset, long long length) Verbindungs Fehler");
												//			}
												//			con.woutput += count;
												//			sent += count;
												//		}
												//	}
												//	interrupt.store(false);
												//	readfileasync.join();
												//}
											}
										}
										//else
										//{
										//	search = "content-type";
										//	res = std::search(stream.headerlist.begin(), stream.headerlistend, &search, &search + 1, HPack::KeySearch<std::string, std::string>);
										//	if (res != stream.headerlistend)
										//	{
										//		struct
										//		{
										//			std::string contentSeperator;
										//			std::ofstream fileStream;
										//			fs::path serverPath;
										//		} args;
										//		args.contentSeperator = "--" + Http::Values(res->second)["boundary"];
										//		args.serverPath = filepath;
										//		stream.datahandler = [&con, &args](Frame & frame)->void {
										//			int contentSeperatorI = buffer.indexof(args.contentSeperator);
										//			if (args.fileStream.is_open() && (contentSeperatorI == -1 || contentSeperatorI > 2))
										//			{
										//				const int data = contentSeperatorI == -1 ? (buffer.length() - args.contentSeperator.length() + 1) : (contentSeperatorI - 2);
										//				buffer.MoveTo(args.fileStream, data);
										//				return contentSeperatorI == -1 ? 0 : 2;
										//			}
										//			else if (contentSeperatorI != -1)
										//			{
										//				int offset = contentSeperatorI + args.contentSeperator.length(), fileHeader = buffer.indexof("\r\n\r\n", 4, offset);
										//				if (fileHeader != -1)
										//				{
										//					if (args.fileStream.is_open())
										//					{
										//						args.fileStream.close();
										//					}
										//					std::string name = Http::Parameter(std::string(buffer.begin() + offset + 2, buffer.begin() + fileHeader))["Content-Disposition"]["name"];
										//					args.fileStream.open((args.serverPath / name.substr(1, name.length() - 2)), std::ios::binary);
										//					return fileHeader + 4;
										//				}
										//				fileHeader = buffer.indexof("--\r\n", 4, offset);
										//				if (fileHeader != -1)
										//				{
										//					if (args.fileStream.is_open())
										//					{
										//						args.fileStream.close();
										//					}
										//					auto &responseHeader = buffer.response();
										//					responseHeader.status = Ok;
										//					buffer.Send(responseHeader.toString());
										//					return ~(fileHeader + 4);
										//				}
										//			}
										//		};
										//		break;
										//	}
										//}
									}
								}
								else
								{
									*headerlist.begin() = { ":status", "404" };
									headerlistend = headerlist.begin() + 1;
									con.winput += 3;
									*con.winput++ = (unsigned char)Frame::Type::HEADERS;
									*con.winput++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
									con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + sizeof(frame.streamIndentifier), con.winput);
									con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlistend);
									{
										uint32_t l = (con.winput - con.woutput) - 9;
										std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
									}
									do
									{
										con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
									} while (con.woutput != con.winput);
									break;
								}
							}
						}
						else
						{
							//__debugbreak();
						}
						break;
					}
					case Frame::Type::PRIORITY:
					{
						Stream::Priority &priority = frame.streamIndentifier == 0 ? con.priority : std::search(con.streams.begin(), con.streamsend, &frame.streamIndentifier, &frame.streamIndentifier + 1, StreamSearch)->priority;
						std::reverse_copy(con.routput, con.routput + 4, (unsigned char*)&priority.streamDependency);
						con.routput += 4;
						priority.weight = *con.routput++;
						break;
					}
					case Frame::Type::RST_STREAM:
					{
						con.streams[0].state = Stream::State::closed;
						con.routput += frame.length;
						break;
					}
					case Frame::Type::SETTINGS:
					{
						con.routput += frame.length;
						uint32_t l = 0;
						con.winput = std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.winput);
						*con.winput++ = (unsigned char)Frame::Type::SETTINGS;
						*con.winput++ = (unsigned char)Frame::Flags::ACK;
						con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + 4, con.winput);
						do
						{
							con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
						} while (con.woutput != con.winput);
						break;
					}
					case Frame::Type::PUSH_PROMISE:
					{
						con.routput += frame.length;
						break;
					}
					case Frame::Type::PING:
					{
						if (frame.flags == Frame::Flags::ACK)
						{
							con.routput += frame.length;
						}
						else
						{
							con.winput = std::reverse_copy((unsigned char*)&frame.length, (unsigned char*)&frame.length + 3, con.winput);
							*con.winput++ = (unsigned char)Frame::Type::PING;
							*con.winput++ = (unsigned char)Frame::Flags::ACK;
							con.winput = std::reverse_copy((unsigned char*)&frame.streamIndentifier, (unsigned char*)&frame.streamIndentifier + sizeof(frame.streamIndentifier), con.winput);
							con.winput = std::copy(con.routput, con.routput + frame.length, con.winput);
							con.routput += frame.length;
							do
							{
								con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
							} while (con.woutput != con.winput);
						}
						break;
					}
					case Frame::Type::GOAWAY:
					{
						con.routput += frame.length;
						con.rmtx.lock();
						if (con.csocket != -1)
						{
							SSL_free(con.cssl);
							FD_CLR(con.csocket, &sockets);
							Http::CloseSocket(con.csocket);
							con.cssl = 0;
						}
						for (; connectionsbegin != connectionsend && connectionsbegin->csocket == -1; ++connectionsbegin);
						con.rmtx.unlock();
						break;
					}
					case Frame::Type::WINDOW_UPDATE:
					{
						uint32_t WindowSizeIncrement = (con.routput[0] << 24) | (con.routput[1] << 16) | (con.routput[2] << 8) | con.routput[3];
						con.routput += frame.length;
						break;
					}
					case Frame::Type::CONTINUATION:
					{
						con.routput += frame.length;
						break;
					}
					default:
					{
						con.routput += frame.length;
					}
					}
					if (con.routput != frameend)
					{
						//__debugbreak();
					}
					timeout = { 0,0 };
					con.wmtx.unlock();
				}
			}
		}
	}
}