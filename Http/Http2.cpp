#include "Http2.h"
#include "Http.h"
#include "hpack.h"
#include "utility.h"
#include "MimeType.h"

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

using namespace Http::V2;
using namespace HPack;
using namespace Utility;
using namespace std::chrono_literals;
namespace fs = std::experimental::filesystem;

std::string ErrorCodes[] = {
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

Stream::Stream(uint32_t indentifier)
{
	this->indentifier = indentifier;
	this->state = State::idle;
}

Stream::Priority::Priority()
{
	dependency = 0;
	weight = 0;
}

Connection::Connection()
{
	uint32_t ssettings[] = { 4096, 1, std::numeric_limits<uint32_t>::max(), 65535, 16384, std::numeric_limits<uint32_t>::max() };
	memcpy(this->settings, ssettings, sizeof(settings));
}

Connection::Connection(uintptr_t csocket, const sockaddr_in6 &address) : Connection()
{
	this->csocket = csocket;
	this->address = address;
}

Connection::Connection(Connection&& con)
{
	this->address = con.address;
	this->csocket = con.csocket;
	this->cssl = con.cssl;
	{
		std::shared_lock<std::shared_mutex> lock(con.streamlock);
		this->streams = std::move(con.streams);
	}
	this->hdecoder = std::move(con.hdecoder);
	this->hencoder = std::move(con.hencoder);
	memcpy(this->settings, con.settings, sizeof(settings));
}

Connection::Connection(const Connection& con)
{
	this->address = con.address;
	this->csocket = con.csocket;
	this->cssl = con.cssl;
	this->streams = con.streams;
	this->hdecoder = con.hdecoder;
	this->hencoder = con.hencoder;
	memcpy(this->settings, con.settings, sizeof(settings));
}

Connection::~Connection()
{
	std::lock(rmtx, wmtx, streamlock);
	rmtx.unlock();
	wmtx.unlock();
	streamlock.unlock();
}

Connection & Connection::operator=(const Connection & con)
{
	this->address = con.address;
	this->csocket = con.csocket;
	this->cssl = con.cssl;
	this->streams = con.streams;
	this->hdecoder = con.hdecoder;
	this->hencoder = con.hencoder;
	memcpy(this->settings, con.settings, sizeof(settings));
	return *this;
}

Server::Server(const std::experimental::filesystem::path & privatekey, const std::experimental::filesystem::path & publiccertificate, const std::experimental::filesystem::path & webroot) : conhandler(std::thread::hardware_concurrency())
{
#ifdef _WIN32
	{
		WSADATA wsaData;
		WSAStartup(WINSOCK_VERSION, &wsaData);
	}
#endif
	{
		if (!fs::is_regular_file(privatekey) || !fs::is_regular_file(publiccertificate))
		{
			throw std::runtime_error("Zertifikat(e) nicht gefunden\nServer kann nicht gestartet werden");
		}
		OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
		sslctx = SSL_CTX_new(TLS_server_method());
		if (SSL_CTX_use_certificate_chain_file(sslctx, publiccertificate.u8string().data()) < 0) {
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
		val = 1;
		if (setsockopt(ssocket, IPPROTO_TCP, TCP_FASTOPEN, (const char*)&val, sizeof(val)) != 0)
		{
			throw std::runtime_error(u8"TCP_FASTOPEN konnte nicht aktiviert werden");
		}
	}
	{
		sockaddr_in6 serverAdresse;
		memset(&serverAdresse, 0, sizeof(sockaddr_in6));
		serverAdresse.sin6_family = AF_INET6;
		serverAdresse.sin6_addr = in6addr_any;
		serverAdresse.sin6_port = htons(443);
		if (::bind(ssocket, (sockaddr *)&serverAdresse, sizeof(serverAdresse)) == -1)
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
	for (int i = 0; i < conhandler.size(); ++i)
	{
		conhandler[i] = std::thread(&Server::connectionshandler, this);
	}
}

const std::experimental::filesystem::path & Server::getWebroot()
{
	return webroot;
}

Server::~Server()
{
	running = false;
	{
		std::shared_lock<std::shared_mutex> conslock(cons);
		for (Connection & con : connections)
		{
			std::unique_lock<std::mutex> lock(con.rmtx);
			if (con.csocket != -1)
			{
				SSL_shutdown(con.cssl);
				SSL_free(con.cssl);
				con.cssl = nullptr;
				Http::CloseSocket(con.csocket);
			}
		}
		for (int i = 0; i < conhandler.size(); ++i)
		{
			if (conhandler[i].joinable())
			{
				conhandler[i].join();
			}
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
	WSACleanup();
}

bool FindFile(fs::path &filepath, std::string &uri)
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
				uri = std::string();
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

bool ReadUntil(SSL *ssl, uint8_t * buffer, int length)
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

Stream & Server::GetStream(std::vector<Stream> &streams, std::shared_mutex &streamsmtx, uint32_t streamIndentifier)
{
	while (true)
	{
		auto res = std::find_if(streams.begin(), streams.end(), [streamIndentifier](const Stream &stream) -> bool { return stream.indentifier == streamIndentifier; });
		if (res == streams.end())
		{
			std::unique_lock<std::shared_mutex> lock(streamsmtx);
			streams.push_back(streamIndentifier);
		}
		else
		{
			return *res;
		}
	}
}

void Server::filehandler(RequestContext &rcontext)
{
	std::vector<std::pair<std::string, std::string>> headerlist;
	if (FindFile(rcontext.path, rcontext.uri))
	{
		fs::path ext = rcontext.path.extension();
		if (ext == ".dll")
		{
			auto res = std::find_if(libs.begin(), libs.end(), [&rcontext](const std::tuple<std::experimental::filesystem::path, void*, void(*)(RequestContext &)> & left) {
				return std::get<0>(left) == rcontext.path;
			});
			void(*requesthandler)(RequestContext &);
			if (res == libs.end())
			{
				std::unique_lock<std::mutex> lock(libsmtx);
				std::tuple<std::experimental::filesystem::path, void*, void(*)(RequestContext &)> entry;
				auto &lib = std::get<1>(entry) = dlopen(rcontext.path.c_str());
				auto init = (void(*)())dlsym(lib, initsymbol);
				if (init != nullptr) init();
				requesthandler = std::get<2>(entry) = (void(*)(RequestContext &))dlsym(lib, mainsymbol);
				if (requesthandler == nullptr)
				{
					dlclose(lib);
					throw std::runtime_error(u8"Datei kann nicht ausgeführt werden");
				}
				std::get<0>(entry) = rcontext.path;
				libs.push_back(std::move(entry));
			}
			else
			{
				requesthandler = std::get<2>(*res);
			}
			requesthandler(rcontext);
		}
		else
		{
			auto res = std::find_if(rcontext.stream->headerlist.begin(), rcontext.stream->headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":method"; });
			if (res != rcontext.stream->headerlist.end() && (res->second == Http::Get || res->second == Http::Head))
			{
				{
					long long etag = fs::last_write_time(rcontext.path).time_since_epoch().count();
					res = std::find_if(rcontext.stream->headerlist.begin(), rcontext.stream->headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "if-none-match"; });
					if (res != rcontext.stream->headerlist.end() && std::stoll(res->second) == etag)
					{
						headerlist.clear();
						headerlist.push_back({ ":status", "304" });
						std::vector<uint8_t> buffer(9 + 10);
						auto wpos = buffer.begin();
						wpos += 3;
						*wpos++ = (unsigned char)Frame::Type::HEADERS;
						*wpos++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
						wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
						wpos = rcontext.connection.hencoder.Headerblock(wpos, headerlist);
						{
							uint32_t length = wpos - buffer.begin() - 9;
							std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
						}
						{
							std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
							SSL_write(rcontext.connection.cssl, buffer.data(), wpos - buffer.begin());
						}
						return;
					}
					headerlist.push_back({ ":status", "200" });
					headerlist.push_back({ "etag", std::to_string(etag) });
				}
				{
					uintmax_t offset = 0, length = fs::file_size(rcontext.path);
					res = std::find_if(rcontext.stream->headerlist.begin(), rcontext.stream->headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "range"; });
					if (res != rcontext.stream->headerlist.end())
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
							std::vector<uint8_t> buffer(9 + rcontext.connection.settings[(uint32_t)Settings::MAX_FRAME_SIZE]);
							auto wpos = buffer.begin();
							wpos += 3;
							*wpos++ = (unsigned char)Frame::Type::HEADERS;
							*wpos++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
							wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
							wpos = rcontext.connection.hencoder.Headerblock(wpos, headerlist);
							{
								uint32_t l = (wpos - buffer.begin()) - 9;
								std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, buffer.begin());
							}
							{
								std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
								SSL_write(rcontext.connection.cssl, buffer.data(), wpos - buffer.begin());
							}
							return;
						}
					}
					headerlist.push_back({ "content-length", std::to_string(length) });
					{
						headerlist.push_back({ "content-type", MimeType::Get(rcontext.path.extension().u8string()) });
					}
					if (rcontext.query == "herunterladen")
					{
						headerlist.push_back({ "content-disposition", "attachment" });
					}
					{
						std::ifstream fstream = std::ifstream(rcontext.path, std::ios::binary);
						if (!fstream.is_open()) throw std::runtime_error(u8"Datei kann nicht ge�ffnet werden");
						std::vector<uint8_t> buffer(9 + rcontext.connection.settings[(uint32_t)Settings::MAX_FRAME_SIZE]);
						auto wpos = buffer.begin();
						wpos += 3;
						*wpos++ = (unsigned char)Frame::Type::HEADERS;
						*wpos++ = (unsigned char)Frame::Flags::END_HEADERS;
						wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
						wpos = rcontext.connection.hencoder.Headerblock(wpos, headerlist);
						{
							uint32_t length = (wpos - buffer.begin()) - 9;
							std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
						}
						{
							std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
							SSL_write(rcontext.connection.cssl, buffer.data(), wpos - buffer.begin());
							buffer.resize(9 + rcontext.connection.settings[(uint32_t)Settings::MAX_FRAME_SIZE]);
							for (long long proc = 0; proc < length;)
							{
								auto wpos = buffer.begin();
								uint64_t count = std::min((uint64_t)(length - proc), (uint64_t)buffer.size() - 9);
								wpos = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, wpos);
								*wpos++ = (unsigned char)Frame::Type::DATA;
								*wpos++ = count == (length - proc) ? (unsigned char)Frame::Flags::END_STREAM : 0;
								wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
								fstream.read((char*)buffer.data() + 9, count);
								wpos += count;
								proc += count;
								SSL_write(rcontext.connection.cssl, buffer.data(), wpos - buffer.begin());
							}
						}
					}
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
		wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
		wpos = rcontext.connection.hencoder.Headerblock(wpos, headerlist);
		{
			uint32_t length = (wpos - buffer.begin()) - 9;
			std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
		}
		{
			std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
			SSL_write(rcontext.connection.cssl, buffer.data(), wpos - buffer.begin());
		}
	}
	switch (rcontext.stream->state)
	{
	case Stream::State::half_closed_remote:
		rcontext.stream->state = Stream::State::closed;
		break;
	case Stream::State::open:
		rcontext.stream->state = Stream::State::half_closed_local;
		break;
	}
}

void Server::processHeaderblock(RequestContext& rcontext)
{
	auto res = std::find_if(rcontext.stream->headerlist.begin(), rcontext.stream->headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":path"; });
	if (res != rcontext.stream->headerlist.end())
	{
		rcontext.path = rcontext.server.getWebroot();
		{
			size_t pq = res->second.find('?');
			if (pq != std::string::npos)
			{
				rcontext.query = Utility::urlDecode(String::Replace(res->second.substr(pq + 1), "+", " "));
				rcontext.path /= Utility::urlDecode(res->second.substr(0, pq));
			}
			else
			{
				rcontext.path /= Utility::urlDecode(res->second);
			}
		}
		rcontext.server.filehandler(rcontext);
	}
}

void Server::framehandler(RequestContext & rcontext)
{
	if (((uint8_t)rcontext.frame.type > (uint8_t)Frame::Type::CONTINUATION) || (rcontext.frame.length > rcontext.connection.settings[(uint16_t)Settings::MAX_FRAME_SIZE]))
	{
		throw std::runtime_error("Fehler: Invalid Frame");
	}
	std::vector<uint8_t> fcontent(rcontext.frame.length);
	if (!ReadUntil(rcontext.connection.cssl, fcontent.data(), fcontent.size()))
	{
		throw std::runtime_error("Verbindungs Fehler");
	}
	{
		rcontext.stream = &GetStream(rcontext.connection.streams, rcontext.connection.streamlock, rcontext.frame.streamIndentifier);
		std::shared_lock<std::shared_mutex> readlock(rcontext.connection.streamlock);
		auto pos = fcontent.begin(), end = fcontent.end();
		switch (rcontext.frame.type)
		{
		case Frame::Type::HEADERS:
		{
			uint8_t padlength = 0;
			if (!((uint8_t)rcontext.frame.flags & (uint8_t)Frame::Flags::END_HEADERS))
			{
				rcontext.conrlock.unlock();
			}
			if ((uint8_t)rcontext.frame.flags & (uint8_t)Frame::Flags::END_STREAM)
			{
				rcontext.conrlock.unlock();
				rcontext.stream->state = Stream::State::half_closed_remote;
			}
			else
			{
				rcontext.stream->state = Stream::State::open;
			}
			if ((uint8_t)rcontext.frame.flags & (uint8_t)Frame::Flags::PADDED)
			{
				padlength = *pos++;
			}
			if ((uint8_t)rcontext.frame.flags & (uint8_t)Frame::Flags::PRIORITY)
			{
				std::reverse_copy(pos, pos + 4, (unsigned char*)&rcontext.stream->priority.dependency);
				pos += 4;
				rcontext.stream->priority.weight = *pos++;
			}
			pos = rcontext.connection.hdecoder.Headerblock(pos, end - padlength, rcontext.stream->headerlist);
			pos += padlength;
			if ((uint8_t)rcontext.frame.flags & (uint8_t)Frame::Flags::END_HEADERS)
			{
				processHeaderblock(rcontext);
			}
			break;
		}
		case Frame::Type::PRIORITY:
		{
			rcontext.conrlock.unlock();
			Stream::Priority &priority = rcontext.stream->priority;
			std::reverse_copy(pos, pos + 4, (unsigned char*)&priority.dependency);
			pos += 4;
			priority.weight = *pos++;
			break;
		}
		case Frame::Type::RST_STREAM:
		{
			rcontext.conrlock.unlock();
			ErrorCode code;
			std::reverse_copy(pos, pos + 4, (unsigned char*)&code);
			pos += 4;
			std::cout << "ErrorCode: " << ErrorCodes[(uint32_t)code] << "(" << (uint32_t)code << ")\n";
			break;
		}
		case Frame::Type::SETTINGS:
		{
			rcontext.conrlock.unlock();
			if (rcontext.frame.flags != Frame::Flags::ACK)
			{
				while (pos != end)
				{
					uint16_t id;
					uint32_t value;
					std::reverse_copy(pos, pos + 2, (char*)&id);
					pos += 2;
					std::reverse_copy(pos, pos + 4, (char*)&value);
					pos += 4;
					rcontext.connection.settings[id - 1] = value;
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
					wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
					{
						std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
						SSL_write(rcontext.connection.cssl, buffer.data(), buffer.size());
					}
				}
			}
			break;
		}
		case Frame::Type::PING:
		{
			rcontext.conrlock.unlock();
			if (rcontext.frame.flags != Frame::Flags::ACK)
			{
				std::vector<uint8_t> buffer(9 + rcontext.frame.length);
				auto wpos = buffer.begin();
				wpos = std::reverse_copy((unsigned char*)&rcontext.frame.length, (unsigned char*)&rcontext.frame.length + 3, wpos);
				*wpos++ = (unsigned char)Frame::Type::PING;
				*wpos++ = (unsigned char)Frame::Flags::ACK;
				wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
				wpos = std::copy(pos, end, wpos);
				pos = end;
				{
					std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
					SSL_write(rcontext.connection.cssl, buffer.data(), buffer.size());
				}
			}
			break;
		}
		case Frame::Type::GOAWAY:
		{
			uint32_t laststreamid;
			ErrorCode code;
			std::reverse_copy(pos, pos + 4, (unsigned char*)&laststreamid);
			pos += 4;
			std::reverse_copy(pos, pos + 4, (unsigned char*)&code);
			pos += 4;
			std::cout << "GOAWAY: Letzter funktionierender Stream: " << laststreamid << "\n";
			std::cout << "ErrorCode: " << ErrorCodes[(uint32_t)code] << "(" << (uint32_t)code << ")\n";
			if (rcontext.frame.length > 8)
			{
				std::cout << "Debug Info: \"" << std::string(pos, pos + (rcontext.frame.length - 8)) << "\"\n";
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
				wpos = std::reverse_copy((unsigned char*)&rcontext.stream->indentifier, (unsigned char*)&rcontext.stream->indentifier + 4, wpos);
				wpos = std::reverse_copy((unsigned char*)&laststreamid, (unsigned char*)&laststreamid + 4, wpos);
				{
					ErrorCode error = ErrorCode::NO_ERROR;
					wpos = std::reverse_copy((unsigned char*)&error, (unsigned char*)&error + 4, wpos);
				}
				{
					std::unique_lock<std::mutex> lock(rcontext.connection.wmtx);
					auto res = SSL_write(rcontext.connection.cssl, buffer.data(), buffer.size());
				}
			}
			throw std::runtime_error("GOAWAY Frame");
		}
		case Frame::Type::WINDOW_UPDATE:
		{
			rcontext.conrlock.unlock();
			uint32_t WindowSizeIncrement = (pos[0] << 24) | (pos[1] << 16) | (pos[2] << 8) | pos[3];
			pos += 4;
			break;
		}
		case Frame::Type::CONTINUATION:
			rcontext.conrlock.unlock();
			pos = rcontext.connection.hdecoder.Headerblock(pos, end, rcontext.stream->headerlist);
			if ((uint8_t)rcontext.frame.flags & (uint8_t)Frame::Flags::END_HEADERS)
			{
				processHeaderblock(rcontext);
			}
			break;
		default:
			rcontext.conrlock.unlock();
			break;
		}
		if (pos != end)
		{
			throw std::runtime_error("Frame procces Error");
		}
	}
}

void Server::connectionshandler()
{
	timeval timeout = { 0,0 };
	size_t nfds = 1024;
	while (running)
	{
		std::unique_lock<std::mutex> polllock(xtm, std::try_to_lock);
		if (polllock.owns_lock())
		{
			{
			pollagain:
				fd_set tmp = sockets;
				if ((activec = select(nfds, &tmp, nullptr, nullptr, &timeout)) <= 0)
				{
					timeout = { 0, 100000 };
					goto pollagain;
				}
				active = tmp;
				if(--activec > 0) polling.notify_one();
			}
			if (FD_ISSET(ssocket, &active))
			{
				FD_CLR(ssocket, &active);
				Connection connection;
				{
					socklen_t sockaddr_len = sizeof(sockaddr_in6);
					connection.csocket = accept(ssocket, (sockaddr *)&connection.address, &sockaddr_len);
					polllock.unlock();
					if (connection.csocket == -1)
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
						//{
						//	char str[INET6_ADDRSTRLEN];
						//	inet_ntop(AF_INET6, &(connection.address.sin6_addr), str, INET6_ADDRSTRLEN);
						//	std::cout << str << ":" << ntohs(connection.address.sin6_port) << " Verbunden\n";
						//}
						{
							std::unique_lock<std::shared_mutex> conslock(cons);
							connections.push_back(std::move(connection));
						}
					}
				}
				timeout = { 0,0 };
			}
			else
			{
				polllock.unlock();
				goto procdata;
			}
		}
		else
		{
		procdata:
			if (activec <= 0)
			{
				std::this_thread::sleep_for(100ms);
				continue;
			}
			timeout = { 0,1000 };
			std::shared_lock<std::shared_mutex> conslock(cons);
			for (auto _it = connections.begin(), _end = connections.end(); _it != _end; ++_it)
			{
				Connection & con = *_it;
				RequestContext rcontext = { *this, con, std::unique_lock<std::mutex>(con.rmtx, std::try_to_lock) };
				if (rcontext.conrlock.owns_lock())
				{
					if (FD_ISSET(con.csocket, &active) || (con.cssl != 0 && SSL_pending(con.cssl)))
					{
						FD_CLR(con.csocket, &active);
						try
						{
							std::vector<uint8_t> framebuf(9);
							if (!ReadUntil(con.cssl, framebuf.data(), framebuf.size()))
							{
								throw std::runtime_error("Verbindungs Fehler");
							}
							{
								rcontext.frame = ReadFrame(framebuf.begin());
								framehandler(rcontext);
							}
						}
						catch (std::exception & ex)
						{
							if (con.cssl != 0)
							{
								FD_CLR(con.csocket, &sockets);
								std::cout << "std::exception what() = \"" << ex.what() << "\"\n";
								std::unique_lock<std::mutex> lock(con.wmtx, std::try_to_lock);
								if (lock.owns_lock())
								{
									lock.unlock();
									if (rcontext.conrlock.owns_lock())
									{
										rcontext.conrlock.unlock();
									}
									conslock.unlock();
									std::unique_lock<std::shared_mutex> ulock(cons);
									if (con.cssl != 0)
									{
										SSL_shutdown(con.cssl);
										SSL_free(con.cssl);
										con.cssl = 0;
										Http::CloseSocket(con.csocket);
										connections.erase(_it);
										//{
										//	char str[INET6_ADDRSTRLEN];
										//	inet_ntop(AF_INET6, &(con.address.sin6_addr), str, INET6_ADDRSTRLEN);
										//	std::cout << str << ":" << ntohs(con.address.sin6_port) << " Getrennt\n";
										//}
									}
									break;
								}
							}
						}
						timeout = { 0,0 };
					}
				}
			}
		}
	}
}