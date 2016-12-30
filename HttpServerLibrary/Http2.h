#pragma once
#include "Array.h"
#include "hpack.h"

#include <iostream>
#include <openssl/ssl.h>
#include <thread>
#include <mutex>
#include <experimental/filesystem>

#ifdef _WIN32
#include <ws2tcpip.h>
#undef min
#undef max
#define SHUT_RDWR SD_BOTH
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <signal.h>
#include <netinet/in.h>
#define closesocket(socket) close(socket)
#endif

namespace Http2
{
	extern std::pair<std::experimental::filesystem::path, std::string> MimeTypeTable[];

	struct Frame
	{
		enum class Flags : uint8_t
		{
			END_STREAM = 0x1,
			ACK = 0x1,
			END_HEADERS = 0x4,
			PADDED = 0x8,
			PRIORITY = 0x20
		};
		enum class Type : uint8_t
		{
			DATA = 0x0,
			HEADERS = 0x1,
			PRIORITY = 0x2,
			RST_STREAM = 0x3,
			SETTINGS = 0x4,
			PUSH_PROMISE = 0x5,
			PING = 0x6,
			GOAWAY = 0x7,
			WINDOW_UPDATE = 0x8,
			CONTINUATION = 0x9
		};
		uint32_t length;
		Type type;
		Flags flags;
		uint32_t streamIndentifier;
	};

	struct Setting
	{
		enum class Id : uint16_t
		{
			SETTINGS_HEADER_TABLE_SIZE = 0x1,
			SETTINGS_ENABLE_PUSH = 0x2,
			SETTINGS_MAX_CONCURRENT_STREAMS = 0x3,
			SETTINGS_INITIAL_WINDOW_SIZE = 0x4,
			SETTINGS_MAX_FRAME_SIZE = 0x5,
			SETTINGS_MAX_HEADER_LIST_SIZE = 0x6
		};
		Id id;
		uint32_t value;
	};

	//class Stream
	//{
	//private:
	//	Array<unsigned char> _array;
	//	RotateIterator::Ranges ranges;
	//	RotateIterator inputiterator;
	//	RotateIterator outputiterator;
	//	void* receiver;
	//	bool running;
	//	uintptr_t csocket;
	//	uintptr_t cssl;
	//public:
	//	Stream(uintptr_t csocket, uintptr_t cssl);
	//	~Stream();
	//	Stream & operator >> (Frame &frame);
	//	Stream & operator << (const Frame &frame);
	//	Stream & operator >> (Headers &headers);
	//	Stream & operator >> (HeadersPadded & headers);
	//	Stream & operator << (const Headers &headers);
	//	Stream & operator >> (Setting &setting);
	//	Stream & operator << (const Setting &setting);
	//	void Release(long long count);
	//	RotateIterator &begin();
	//	RotateIterator end();
	//};

	struct Stream
	{
		enum class State
		{
			idle,
			reserved_local,
			reserved_remote,
			open,
			half_closed_local,
			half_closed_remote,
			closed
		};
		struct Priority
		{
			uint32_t streamDependency;
			uint8_t weight;
		};
		State state;
		Priority priority;
		uint32_t streamIndentifier;
		Utility::Array<std::pair<std::string, std::string>> headerlist;
		Utility::RotateIterator<std::pair<std::string, std::string>> headerlistend;
		std::function<void(Frame&)> datahandler;
	};

	struct Connection
	{
		std::mutex rmtx, wmtx;
		uintptr_t csocket;
		sockaddr_in6 adresse;
		SSL * cssl;
		int pending;
		Stream::Priority priority;
		Utility::Array<uint8_t> rbuf, wbuf;
		Utility::RotateIterator<uint8_t> rinput, routput;
		Utility::RotateIterator<uint8_t> winput, woutput;
		Utility::Array<Stream> streams;
		Utility::RotateIterator<Stream> streamsend;
		HPack::Encoder hencoder;
		HPack::Decoder hdecoder;
	};

	class Server
	{
	private:
		SSL_CTX* sslctx;
		uintptr_t ssocket;
		fd_set sockets, active;
		Utility::Array<std::thread> conhandler;
		Utility::Array<Connection> connections;
		Utility::RotateIterator<Connection> connectionsbegin;
		Utility::RotateIterator<Connection> connectionsend;
		bool running;
		std::experimental::filesystem::path rootpath;
		Utility::Array<std::tuple<std::experimental::filesystem::path, void*, void(*)(Connection&, Stream&, const std::experimental::filesystem::path&, const std::string&, const std::string&)>> libs;
		Utility::RotateIterator<std::tuple<std::experimental::filesystem::path, void*, void(*)(Connection&, Stream&, const std::experimental::filesystem::path&, const std::string&, const std::string&)>> libsend;
		std::mutex libsmtx;
		void connectionshandler();
	public:
		Server(const std::experimental::filesystem::path &certroot, const std::experimental::filesystem::path & rootpath);
		~Server();
	};
}