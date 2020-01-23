#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <condition_variable>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <WS2tcpip.h>
#undef NO_ERROR
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#define closesocket(socket) close(socket)
typedef int SOCKET;
#endif

namespace Net
{
	class Socket
	{
	protected:
		SOCKET handle;
		std::shared_ptr<sockaddr> socketaddress;
		std::string protocol;
		std::mutex readlock;
		std::mutex writelock;
		virtual int Receive(uint8_t * buffer, int length);
		virtual int Send(const uint8_t * buffer, int length);
	public:
		class SocketInputStream
		{
		private:
			Socket* handle;
			std::unique_lock<std::mutex> lock;
		public:
			SocketInputStream(Socket * handle, std::unique_lock<std::mutex> && lock);
			int Receive(uint8_t * buffer, int length);
			bool ReceiveAll(uint8_t * buffer, int length);
		};

		class SocketOutputStream
		{
		private:
			Socket & handle;
			std::unique_lock<std::mutex> lock;
		public:
			SocketOutputStream(Socket & handle, std::unique_lock<std::mutex> && lock);
			int Send(const uint8_t * buffer, int length);
			int Send(std::vector<uint8_t> buffer);
			int Send(std::vector<uint8_t> buffer, int length);
			bool SendAll(const uint8_t * buffer, int length);
			bool SendAll(std::vector<uint8_t> buffer);
			bool SendAll(std::vector<uint8_t> buffer, int length);
		};
		Socket(Socket && socket);
		Socket(SOCKET socket, const std::shared_ptr<sockaddr> & socketaddress);
		virtual ~Socket();
		static std::shared_ptr<Socket> Connect(const std::string address, short port);
		SocketOutputStream GetOutputStream();
		SocketInputStream GetInputStream();
		SOCKET GetHandle();
		std::string GetAddress();
		uint16_t GetPort();
		void SetProtocol(const std::string & protocol);
		std::string GetProtocol();
		virtual int SendTo(uint8_t * buffer, int length, const sockaddr * to, socklen_t tolength);
		virtual int ReceiveFrom(uint8_t * buffer, int length, sockaddr * from, socklen_t * fromlength);
	};
}