#pragma once
#include <cstdint>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WS2tcpip.h>
#undef max
#undef min
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
		in6_addr address;
		int port;
		std::string protocol;
	public:
		Socket();
		Socket(Socket && socket);
		Socket(SOCKET socket, const in6_addr &address, int port);
		Socket(SOCKET socket, const SOCKADDR_STORAGE &address);
		virtual ~Socket();
		static Socket Accept(SOCKET listener);
		SOCKET GetSocket();
		const in6_addr &GetAddress();
		int GetPort();
		void SetProtocol(const std::string & protocol);
		std::string GetProtocol();
		virtual int Receive(uint8_t * buffer, int length);
		void ReceiveAll(uint8_t * buffer, int length);
		virtual int Send(uint8_t * buffer, int length);
		int Send(std::vector<uint8_t> buffer);
		void SendAll(uint8_t * buffer, int length);
		void SendAll(std::vector<uint8_t> buffer);
		virtual int SendTo(uint8_t * buffer, int length, const sockaddr * to, socklen_t tolength);
		virtual int ReceiveFrom(uint8_t * buffer, int length, sockaddr * from, socklen_t * fromlength);
	};
}