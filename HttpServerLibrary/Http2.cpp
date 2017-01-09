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
#define mainsymbol "?requesthandler@@YAXAEAVServer@Http2@@AEAVConnection@2@AEAVStream@2@AEAVpath@v1@filesystem@experimental@std@@AEAV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@9@4@Z"
#define deinitsymbol "?deinit@@YAXXZ"
#else
#define dlopen(name) dlopen(name,RTLD_NOW);
#define initsymbol "_Z4initv"
#define mainsymbol "_Z14requesthandlerRN5Http26ServerERNS_10ConnectionERNS_6StreamERNSt12experimental10filesystem2v17__cxx114pathERNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEESI_"
#define deinitsymbol "_Z6deinitv"
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
	{ "", "application/octet-stream" }
};

std::string Http2::ErrorCodes[] = {
	"NO_ERROR",
	"PROTOCOL_ERROR",
	"INTERNAL_ERROR",
	"FLOW_CONTROL_ERROR",
	"SETTINGS_TIMEOUT",
	"STREAM_CLOSED",
	"FRAME_SIZE_ERROR",
	"REFUSED_STREAM",
	"CANCEL",
	"COMPRESSION_ERROR",
	"CONNECT_ERROR",
	"ENHANCE_YOUR_CALM",
	"INADEQUATE_SECURITY",
	"HTTP_1_1_REQUIRED"
};

Http2::Server::Server(const std::experimental::filesystem::path & certroot, const std::experimental::filesystem::path & rootpath) : conhandler(std::thread::hardware_concurrency()), running(true), rootpath(rootpath)
{
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
	connections.reserve(10);
	FD_ZERO(&sockets);
	FD_SET(ssocket, &sockets);
	for (int i = 0; i < conhandler.size(); ++i)
	{
		conhandler[i] = std::thread(&Server::connectionshandler, this);
	}
}

const std::experimental::filesystem::path & Http2::Server::GetRootPath()
{
	return rootpath;
}

Server::~Server()
{
	running = false;
	for (int i = 0; i < conhandler.size(); ++i)
	{
		if (conhandler[i].joinable())
		{
			conhandler[i].join();
		}
	}
	for (Connection & con : connections)
	{
		if (con.csocket != -1 && con.rmtx.try_lock())
		{
			SSL_shutdown(con.cssl);
			SSL_free(con.cssl);
			Http::CloseSocket(con.csocket);
			con.rmtx.unlock();
		}
	}
	for (auto &lib : libs)
	{
		void* handle = std::get<1>(lib);
		if (handle != 0)
		{
			auto deinit = (void(*)())dlsym(handle, deinitsymbol);
			if (deinit != nullptr) deinit();
			dlclose(handle);
		}
	}
}

bool Http2::FindFile(fs::path &filepath, std::string &uri)
{
	if (fs::is_regular_file(filepath))
	{
		return true;
	}
	else
	{
		fs::path pathbuilder;
		for (auto pos = filepath.begin(), end = filepath.end(); pos != end; ++pos)
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

bool Http2::ReadUntil(SSL *ssl, uint8_t * buffer, int length)
{
	int fd = SSL_get_fd(ssl);
	timeval tv = { 0,0 };
	fd_set reader;
	FD_ZERO(&reader);
	uint8_t * end = buffer + length;
	while (buffer != end)
	{
		if (SSL_pending(ssl) > 0)
		{
			goto readl;
		}
		FD_SET(fd, &reader);
		select(fd + 1, &reader, nullptr, nullptr, &tv);
		if (FD_ISSET(fd, &reader))
		{
		readl:
			auto res = SSL_read(ssl, buffer, end - buffer);
			if (res <= 0)
			{
				return false;
			}
			buffer += res;
		}
	}
	return true;
}

Stream & GetStream(std::vector<Stream> &streams, uint32_t streamIndentifier)
{
	//std::cout << "Get Stream: " << streamIndentifier << "\n";
	while (true)
	{
		auto res = std::find_if(streams.begin(), streams.end(), [streamIndentifier](const Stream &stream) -> bool { return stream.indentifier == streamIndentifier; });
		if (res == streams.end())
		{
			streams.push_back(Stream(streamIndentifier));
		}
		else
		{
			//std::cout << "Got Stream: " << res->indentifier << "\n";
			return *res;
		}
	}
}

void Http2::Server::filehandler(Server & server, Connection & con, Stream & stream, fs::path & filepath, std::string & uri, std::string & args)
{
	std::vector<std::pair<std::string, std::string>> headerlist;
	if (FindFile(filepath, uri))
	{
		fs::path ext = filepath.extension();
		if (ext == ".so")
		{
			auto res = std::find_if(libs.begin(), libs.end(), [&filepath](const std::tuple<std::experimental::filesystem::path, void*, void(*)(Server &, Connection &, Stream &, std::experimental::filesystem::path &, std::string &, std::string &)> & left) {
				return std::get<0>(left) == filepath;
			});
			void(*requesthandler)(Server &, Connection &, Stream &, fs::path &, std::string &, std::string &);
			if (res == libs.end())
			{
				libsmtx.lock();
				std::tuple<std::experimental::filesystem::path, void*, void(*)(Server &, Connection &, Stream &, std::experimental::filesystem::path &, std::string &, std::string &)> entry;
				auto &lib = std::get<1>(entry) = dlopen(filepath.c_str());
				auto init = (void(*)())dlsym(lib, initsymbol);
				if (init != nullptr) init();
				requesthandler = std::get<2>(entry) = (void(*)(Server &, Connection &, Stream &, fs::path &, std::string &, std::string &))dlsym(lib, mainsymbol);
				if (requesthandler == nullptr)
				{
					dlclose(lib);
					libsmtx.unlock();
					throw std::runtime_error(u8"Datei kann nicht ausgeführt werden");
				}
				std::get<0>(entry) = filepath;
				libs.push_back(std::move(entry));
				libsmtx.unlock();
			}
			else
			{
				requesthandler = std::get<2>(*res);
			}
			requesthandler(server, con, stream, filepath, uri, args);
		}
		else
		{
			auto res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":method"; });
			if (res != stream.headerlist.end() && (res->second == Http::Get || res->second == Http::Head))
			{
				{
					std::vector<char> buf(128);
					time_t date = fs::file_time_type::clock::to_time_t(fs::last_write_time(filepath));
					strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&date));
					res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "if-modified-since"; });
					if (res != stream.headerlist.end() && res->second == buf.data())
					{
						headerlist.clear();
						headerlist.push_back({ ":status", "304" });
						std::vector<uint8_t> buffer(9 + 10);
						auto wpos = buffer.begin();
						wpos += 3;
						*wpos++ = (unsigned char)Frame::Type::HEADERS;
						*wpos++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
						wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
						wpos = con.hencoder.Headerblock(wpos, headerlist);
						{
							uint32_t length = wpos - buffer.begin() - 9;
							std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
						}
						con.wmtx.lock();
						SSL_write(con.cssl, buffer.data(), wpos - buffer.begin());
						con.wmtx.unlock();
						return;
					}
					headerlist.push_back({ ":status", "200" });
					headerlist.push_back({ "last-modified", buf.data() });
					time(&date);
					strftime(buf.data(), buf.size(), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&date));
					headerlist.push_back({ "date", buf.data() });
					headerlist.push_back({ "accept-ranges", "bytes" });
				}
				{
					uintmax_t offset = 0, length = fs::file_size(filepath);
					res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "range"; });
					if (res != stream.headerlist.end())
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
							std::vector<uint8_t> buffer(9 + con.settings[(uint32_t)Settings::MAX_FRAME_SIZE]);
							auto wpos = buffer.begin();
							wpos += 3;
							*wpos++ = (unsigned char)Frame::Type::HEADERS;
							*wpos++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
							wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
							wpos = con.hencoder.Headerblock(wpos, headerlist);
							{
								uint32_t l = (wpos - buffer.begin()) - 9;
								std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, buffer.begin());
							}
							con.wmtx.lock();
							SSL_write(con.cssl, buffer.data(), wpos - buffer.begin());
							con.wmtx.unlock();
							return;
						}
					}
					headerlist.push_back({ "content-length", std::to_string(length) });
					{
						headerlist.push_back({ "content-type", std::find_if(MimeTypeTable, MimeTypeTable + (sizeof(MimeTypeTable) / sizeof(std::pair<fs::path, std::string>) - 1), [&ext](const std::pair<fs::path, std::string> & pair) { return pair.first == ext; })->second });
					}
					if (args == "herunterladen")
					{
						headerlist.push_back({ "content-disposition", "attachment" });
					}
					{
						std::ifstream fstream = std::ifstream(filepath, std::ios::binary);
						if (!fstream.is_open()) throw std::runtime_error(u8"Datei kann nicht ge�ffnet werden");
						std::vector<uint8_t> buffer(9 + con.settings[(uint32_t)Settings::MAX_FRAME_SIZE]);
						auto wpos = buffer.begin();
						wpos += 3;
						*wpos++ = (unsigned char)Frame::Type::HEADERS;
						*wpos++ = (unsigned char)Frame::Flags::END_HEADERS;
						wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
						wpos = con.hencoder.Headerblock(wpos, headerlist);
						{
							uint32_t length = (wpos - buffer.begin()) - 9;
							std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
						}
						con.wmtx.lock();
						SSL_write(con.cssl, buffer.data(), wpos - buffer.begin());
						buffer.resize(9 + con.settings[(uint32_t)Settings::MAX_FRAME_SIZE]);
						for (long long proc = 0; proc < length;)
						{
							auto wpos = buffer.begin();
							uint64_t count = std::min((uint64_t)(length - proc), (uint64_t)buffer.size() - 9);
							wpos = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, wpos);
							*wpos++ = (unsigned char)Frame::Type::DATA;
							*wpos++ = count == (length - proc) ? (unsigned char)Frame::Flags::END_STREAM : 0;
							wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
							fstream.read((char*)buffer.data() + 9, count);
							wpos += count;
							proc += count;
							SSL_write(con.cssl, buffer.data(), wpos - buffer.begin());
							con.wmtx.unlock();
						}
					}
				}
				switch (stream.state)
				{
				case Stream::State::half_closed_remote:
					stream.state = Stream::State::closed;
					break;
				case Stream::State::open:
					stream.state = Stream::State::half_closed_local;
					break;
				}
			}
		}
	}
	else
	{
		headerlist.clear();
		headerlist.push_back({ ":status", "404" });
		std::vector<uint8_t> buffer(9 + 10);
		auto wpos = buffer.begin();
		wpos += 3;
		*wpos++ = (unsigned char)Frame::Type::HEADERS;
		*wpos++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
		wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
		wpos = con.hencoder.Headerblock(wpos, headerlist);
		{
			uint32_t length = (wpos - buffer.begin()) - 9;
			std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
		}
		con.wmtx.lock();
		SSL_write(con.cssl, buffer.data(), wpos - buffer.begin());
		con.wmtx.unlock();
		switch (stream.state)
		{
		case Stream::State::half_closed_remote:
			stream.state = Stream::State::closed;
			break;
		case Stream::State::open:
			stream.state = Stream::State::half_closed_local;
			break;
		}
	}
}

void Server::connectionshandler()
{
	timeval timeout = { 0,0 }, poll = { 0,0 };
	size_t nfds = 1024;
	while (running)
	{
		{
			fd_set tmp = sockets;
			select(nfds, &tmp, nullptr, nullptr, &timeout);
			active = tmp;
		}

		if (FD_ISSET(ssocket, &active))
		{
			FD_CLR(ssocket, &active);
			Connection connection;
			{
				socklen_t sockaddr_len = sizeof(sockaddr_in6);
				if ((connection.csocket = accept(ssocket, (sockaddr *)&connection.address, &sockaddr_len)) == -1)
				{
					continue;
				}
			}
			SSL_set_fd(connection.cssl = SSL_new(sslctx), connection.csocket);
			if (SSL_accept(connection.cssl) != 1)
			{
				SSL_free(connection.cssl);
				Http::CloseSocket(connection.csocket);
				continue;
			}
			{
				const char http2preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
				std::vector<uint8_t> buffer(sizeof(http2preface) - 1);
				if (!ReadUntil(connection.cssl, buffer.data(), buffer.size()) || memcmp(http2preface, buffer.data(), buffer.size()))
				{
					SSL_shutdown(connection.cssl);
					SSL_free(connection.cssl);
					Http::CloseSocket(connection.csocket);
				}
				else
				{
					FD_SET(connection.csocket, &sockets);
					//nfds = std::max(connection.csocket + 1, nfds);
					{
						char str[INET6_ADDRSTRLEN];
						inet_ntop(AF_INET6, &(connection.address.sin6_addr), str, INET6_ADDRSTRLEN);
						std::cout << str << ":" << ntohs(connection.address.sin6_port) << " Verbunden\n";
					}
					connections.push_back(std::move(connection));
				}
			}
			timeout = { 0,0 };
		}
		else
		{
			timeout = { 0,1000 };
			for (Connection & con : connections)
			{
				if ((FD_ISSET(con.csocket, &active) || (con.cssl != 0 && SSL_pending(con.cssl))) && con.rmtx.try_lock())
				{
					FD_CLR(con.csocket, &active);
					try
					{
						std::vector<uint8_t> framebuf(9);
						if (!ReadUntil(con.cssl, framebuf.data(), framebuf.size()))
						{
							con.rmtx.unlock();
							throw std::runtime_error("Verbindungs Fehler");
						}
						{
							Frame frame = ReadFrame(framebuf.begin());
							std::cout << "Read Frame: " << (uint32_t)frame.type << " | " << frame.length << "\n";
							if (((uint8_t)frame.type > (uint8_t)Frame::Type::CONTINUATION) || (frame.length > con.settings[(uint16_t)Settings::MAX_FRAME_SIZE]))
							{
								std::cout << "MAX_FRAME_SIZE=" << con.settings[(uint16_t)Settings::MAX_FRAME_SIZE] << "\n";
								std::cout << "CONTINUATION=" << (uint8_t)Frame::Type::CONTINUATION << "\n";
								con.rmtx.unlock();
								throw std::runtime_error("Fehler: Invalid Frame");
							}
							framebuf.resize(frame.length);
							if (!ReadUntil(con.cssl, framebuf.data(), framebuf.size()))
							{
								con.rmtx.unlock();
								throw std::runtime_error("Verbindungs Fehler");
							}
							{
								Stream & stream = GetStream(con.streams, frame.streamIndentifier);
								auto pos = framebuf.begin(), end = framebuf.end();
								switch (frame.type)
								{
								case Frame::Type::HEADERS:
								{
									uint8_t padlength = 0;
									if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::END_STREAM))
									{
										con.rmtx.unlock();
										stream.state = Stream::State::half_closed_remote;
									}
									else
									{
										stream.state = Stream::State::open;
									}

									if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::PADDED))
									{
										padlength = *pos++;
									}
									if (((uint8_t)frame.flags & (uint8_t)Frame::Flags::PRIORITY))
									{
										std::reverse_copy(pos, pos + 4, (unsigned char*)&stream.priority.dependency);
										pos += 4;
										stream.priority.weight = *pos++;
									}
									pos = con.hdecoder.Headerblock(pos, end - padlength, stream.headerlist);
									pos = end;
									auto res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":path"; });
									if (res != stream.headerlist.end())
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
										std::cout << "res->second: " << res->second << "\n";
										filehandler(*this, con, stream, filepath, uri, args);
									}
									break;
								}
								case Frame::Type::PRIORITY:
								{
									con.rmtx.unlock();
									Stream::Priority &priority = stream.priority;
									std::reverse_copy(pos, pos + 4, (unsigned char*)&priority.dependency);
									pos += 4;
									priority.weight = *pos++;
									break;
								}
								case Frame::Type::RST_STREAM:
								{
									con.rmtx.unlock();
									ErrorCode code;
									std::reverse_copy(pos, pos + 4, (unsigned char*)&code);
									pos += 4;
									std::cout << "ErrorCode: " << ErrorCodes[(uint32_t)code] << "(" << (uint32_t)code << ")\n";
								}
								case Frame::Type::SETTINGS:
								{
									con.rmtx.unlock();
									if (frame.flags != Frame::Flags::ACK)
									{
										while (pos != end)
										{
											uint16_t id;
											uint32_t value;
											std::reverse_copy(pos, pos + 2, (char*)&id);
											pos += 2;
											std::reverse_copy(pos, pos + 4, (char*)&value);
											pos += 4;
											con.settings[id - 1] = value;
										}
										{
											std::vector<uint8_t> buffer(9);
											auto wpos = buffer.begin();
											{
												uint32_t length = 0;
												wpos = std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, wpos);
											}
											*wpos++ = (unsigned char)Frame::Type::SETTINGS;
											*wpos++ = (unsigned char)Frame::Flags::ACK;
											wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
											con.wmtx.lock();
											SSL_write(con.cssl, buffer.data(), buffer.size());
											con.wmtx.unlock();
										}
									}
									break;
								}
								case Frame::Type::PING:
								{
									con.rmtx.unlock();
									if (frame.flags != Frame::Flags::ACK)
									{
										std::vector<uint8_t> buffer(9 + frame.length);
										auto wpos = buffer.begin();
										wpos = std::reverse_copy((unsigned char*)&frame.length, (unsigned char*)&frame.length + 3, wpos);
										*wpos++ = (unsigned char)Frame::Type::PING;
										*wpos++ = (unsigned char)Frame::Flags::ACK;
										wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
										wpos = std::copy(pos, end, wpos);
										pos = end;
										con.wmtx.lock();
										SSL_write(con.cssl, buffer.data(), buffer.size());
										con.wmtx.unlock();
									}
									break;
								}
								case Frame::Type::GOAWAY:
								{
									con.rmtx.unlock();
									uint32_t laststreamid;
									ErrorCode code;
									std::reverse_copy(pos, pos + 4, (unsigned char*)&laststreamid);
									pos += 4;
									std::reverse_copy(pos, pos + 4, (unsigned char*)&code);
									pos += 4;
									std::cout << "GOAWAY: Letzter funktionierender Stream: " << laststreamid << "\n";
									std::cout << "ErrorCode: " << ErrorCodes[(uint32_t)code] << "(" << (uint32_t)code << ")\n";
									if (frame.length > 8)
									{
										std::cout << "Debug Info: \"" << std::string(pos, pos + (frame.length - 8)) << "\"\n";
									}
									{
										std::vector<uint8_t> buffer(9 + 8);
										auto wpos = buffer.begin();
										{
											uint32_t length = 8;
											wpos = std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, wpos);
										}
										*wpos++ = (unsigned char)Frame::Type::GOAWAY;
										*wpos++ = 0;
										wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
										wpos = std::reverse_copy((unsigned char*)&laststreamid, (unsigned char*)&laststreamid + 4, wpos);
										{
											ErrorCode error = ErrorCode::NO_ERROR;
											wpos = std::reverse_copy((unsigned char*)&error, (unsigned char*)&error + 4, wpos);
										}
										con.wmtx.lock();
										SSL_write(con.cssl, buffer.data(), buffer.size());
										con.wmtx.unlock();
									}
									throw std::runtime_error("GOAWAY Frame");
								}
								case Frame::Type::WINDOW_UPDATE:
								{
									con.rmtx.unlock();
									uint32_t WindowSizeIncrement = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3];
									pos += 4;
									break;
								}
								default:
									con.rmtx.unlock();
									break;
								}
								if (pos != end)
								{
									throw std::runtime_error("Frame procces Error");
								}
								timeout = { 0,0 };
							}
						}
					}
					catch (std::exception & ex)
					{
						con.rmtx.lock();
						FD_CLR(con.csocket, &sockets);
						std::cout << "std::exception what() \"" << ex.what() << "\"\n";
						{
							char str[INET6_ADDRSTRLEN];
							inet_ntop(AF_INET6, &(con.address.sin6_addr), str, INET6_ADDRSTRLEN);
							std::cout << str << ":" << ntohs(con.address.sin6_port) << " Getrennt\n";
						}
						if (con.wmtx.try_lock())
						{
							SSL * ssltmp = con.cssl;
							con.cssl = 0;
							SSL_shutdown(ssltmp);
							SSL_free(ssltmp);
							Http::CloseSocket(con.csocket);
							con.wmtx.unlock();
						}
						con.wmtx.unlock();
					}
				}
			}
		}
	}
}

Http2::Stream::Stream(uint32_t indentifier)
{
	this->indentifier = indentifier;
	this->state = State::idle;
}

Http2::Stream::Priority::Priority()
{
	dependency = 0;
	weight = 0;
}

Http2::Connection::Connection()
{
	uint32_t ssettings[] = { 4096, 1, std::numeric_limits<uint32_t>::max(), 65535, 16384, std::numeric_limits<uint32_t>::max() };
	memcpy(this->settings, ssettings, sizeof(settings));
}

Http2::Connection::Connection(uintptr_t csocket, const sockaddr_in6 &address) : Connection()
{
	this->csocket = csocket;
	this->address = address;
}

Http2::Connection::Connection(Http2::Connection&& con)
{
	this->address = con.address;
	this->csocket = con.csocket;
	this->cssl = con.cssl;
	this->hdecoder = std::move(con.hdecoder);
	this->hencoder = std::move(con.hencoder);
	memcpy(this->settings, con.settings, sizeof(settings));
}

Http2::Connection::Connection(const Http2::Connection& con)
{
	this->address = con.address;
	this->csocket = con.csocket;
	this->cssl = con.cssl;
	this->hdecoder = con.hdecoder;
	this->hencoder = con.hencoder;
	memcpy(this->settings, con.settings, sizeof(settings));
}

Http2::Connection::~Connection()
{
	std::lock(rmtx, wmtx);
	rmtx.unlock();
	wmtx.unlock();
}

Connection & Http2::Connection::operator=(const Connection & con)
{
	this->address = con.address;
	this->csocket = con.csocket;
	this->cssl = con.cssl;
	this->hdecoder = con.hdecoder;
	this->hencoder = con.hencoder;
	memcpy(this->settings, con.settings, sizeof(settings));
	return *this;
}
