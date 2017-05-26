#pragma once
#include "Array.h"
#include "hpack.h"

#include <iostream>
#include <openssl/ssl.h>
#include <thread>
#include <mutex>
#include <experimental/filesystem>
#include <shared_mutex>

#ifdef _WIN32
#include <ws2tcpip.h>
#undef min
#undef max
#undef NO_ERROR
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

namespace Http
{
	namespace V2
	{
		namespace HPack
		{
			class Encoder;
			class Decoder;
		}
		struct RequestContext;
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

		enum class ErrorCode : uint32_t
		{
			NO_ERROR,
			PROTOCOL_ERROR,
			INTERNAL_ERROR,
			FLOW_CONTROL_ERROR,
			SETTINGS_TIMEOUT,
			STREAM_CLOSED,
			FRAME_SIZE_ERROR,
			REFUSED_STREAM,
			CANCEL,
			COMPRESSION_ERROR,
			CONNECT_ERROR,
			ENHANCE_YOUR_CALM,
			INADEQUATE_SECURITY,
			HTTP_1_1_REQUIRED
		};

		extern std::string ErrorCodes[];

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
			enum class State : uint8_t
			{
				idle = 0b0,
				reserved_local = 0b00000100,
				reserved_remote = 0b00001000,
				open = 0b00000011,
				half_closed_local = 0b00000001,
				half_closed_remote = 0b00000010,
				closed = 0b11110000
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
			std::vector<Stream> streams;
			std::shared_mutex streamlock;
			HPack::Encoder hencoder;
			HPack::Decoder hdecoder;
			uint32_t settings[6];

			Connection();
			Connection(uintptr_t csocket, const sockaddr_in6 &address);
			Connection(Connection && con);
			Connection(const Connection & con);
			~Connection();
			Connection& operator=(const Connection & con);
		};

		bool FindFile(std::experimental::filesystem::path & filepath, std::string & uri);
		bool ReadUntil(SSL *ssl, uint8_t * buffer, int length);
		template<class Iterator>
		Frame ReadFrame(Iterator iterator)
		{
			Frame frame;
			*std::reverse_copy(iterator, iterator + 3, (unsigned char*)&frame.length) = 0;
			iterator += 3;
			frame.type = (Frame::Type)*iterator++;
			frame.flags = (Frame::Flags)*iterator++;
			std::reverse_copy(iterator, iterator + 4, (unsigned char*)&frame.streamIndentifier);
			iterator += 4;
			return frame;
		}


		class Server
		{
		private:
			bool running;
			SSL_CTX* sslctx;
			uintptr_t ssocket;
			std::mutex libsmtx, xtm, ctm;
			std::shared_mutex cons;
			std::condition_variable polling;
			int activec;
			fd_set sockets, active;
			void connectionshandler();
			std::vector<std::thread> conhandler;
			std::vector<Connection> connections;
			std::mutex connectionsmtx;
			std::experimental::filesystem::path webroot;
			std::vector<std::tuple<std::experimental::filesystem::path, void*, void(*)(RequestContext &)>> libs;
		public:
			Server(const std::experimental::filesystem::path &privatekey, const std::experimental::filesystem::path &publiccertificate, const std::experimental::filesystem::path & webroot);
			const std::experimental::filesystem::path & getWebroot();
			~Server();
			static void processHeaderblock(RequestContext &rcontext);
			static void framehandler(RequestContext &rcontext);
			static Stream & GetStream(std::vector<Stream>& streams, std::shared_mutex & streamsmtx, uint32_t streamIndentifier);
			void filehandler(RequestContext &rcontext);
		};

		struct RequestContext
		{
			Server &server;
			Connection & connection;
			std::unique_lock<std::mutex> conrlock;
			Frame frame;
			Stream * stream;
			std::experimental::filesystem::path path;
			std::string uri;
			std::string query;
		};
	}
}
