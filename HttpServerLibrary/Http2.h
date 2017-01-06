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

/*template<class T1, class T2>
class FindKey
{
private:
        const T1 &key;
public:
        FindKey(const T1& key) : key(key) {}
        bool operator()(const std::pair<T1, T2> &pair)
        {
                return pair.first == key;
        }
};*/

namespace Http2
{
	namespace HPack
	{
		class Encoder;
		class Decoder;
	}
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

	enum class Settings : uint16_t
	{
		HEADER_TABLE_SIZE = 0x0,
		ENABLE_PUSH = 0x1,
		MAX_CONCURRENT_STREAMS = 0x2,
		INITIAL_WINDOW_SIZE = 0x3,
		MAX_FRAME_SIZE = 0x4,
		MAX_HEADER_LIST_SIZE = 0x5
	};

	class Stream
	{
	public:
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
		class Priority
		{
		public:
			Priority();
			uint32_t dependency;
			uint8_t weight;
		};
		State state;
		Priority priority;
		uint32_t indentifier;
		std::vector<std::pair<std::string, std::string>> headerlist;
		std::function<void(Frame&)> datahandler;

		Stream(uint32_t indentifier);
	};

	class Connection
	{
	public:
		std::mutex rmtx, wmtx;
		uintptr_t csocket;
		sockaddr_in6 address;
		SSL * cssl;
		int pending;
		std::vector<uint8_t> rbuf, wbuf;
		Utility::Ranges<uint8_t> rranges, wranges;
		Utility::RotateIterator<uint8_t> rinput, routput;
		Utility::RotateIterator<uint8_t> winput, woutput;
		std::vector<Stream> streams;
		Http2::HPack::Encoder hencoder;
		Http2::HPack::Decoder hdecoder;
		uint32_t settings[6];

		Connection();
		Connection(uintptr_t csocket, sockaddr_in6 address);
		Connection(Http2::Connection && con);
		Connection(const Http2::Connection & con);
		~Connection();
		Connection& operator=(const Connection & con);
	};

	bool FindFile(std::experimental::filesystem::path & filepath, std::string & uri);

	class Server
	{
	private:
		bool running;
		SSL_CTX* sslctx;
		uintptr_t ssocket;
		std::mutex libsmtx;
		fd_set sockets, active;
		void connectionshandler();
		std::vector<std::thread> conhandler;
		std::vector<Connection> connections;
		std::experimental::filesystem::path rootpath;
		std::vector<std::tuple<std::experimental::filesystem::path, void*, void(*)(Server &, Connection &, Stream &, std::experimental::filesystem::path &, std::string &, std::string &)>> libs;
	public:
		Server(const std::experimental::filesystem::path &certroot, const std::experimental::filesystem::path & rootpath);
		~Server();
		const std::experimental::filesystem::path & GetRootPath();
		void filehandler(Server & server, Connection & con, Stream & stream, std::experimental::filesystem::path & filepath, std::string & uri, std::string & args);
		static bool ReadUntil(SSL * ssl, Utility::RotateIterator<uint8_t> & iterator, int length);
		static Frame ReadFrame(Utility::RotateIterator<uint8_t>& iterator);

	};
}
