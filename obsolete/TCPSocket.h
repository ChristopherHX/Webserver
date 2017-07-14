#pragma once
#include <cstdint>
#include <string>
#include <array>
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
#endif

namespace Net
{
	class TCPSocket
	{
	protected:
		intptr_t socket;
		in6_addr address;
		int port;
		std::string protocol;
	public:
		TCPSocket(intptr_t socket, const in6_addr &address, int port);
		virtual ~TCPSocket();
		intptr_t GetSocket();
		const in6_addr &GetAddress();
		int GetPort();
		void SetProtocol(const std::string & protocol);
		std::string GetProtocol();
		virtual int Receive(uint8_t * buffer, int length);
		void ReceiveAll(uint8_t * buffer, int length);
		virtual int Send(uint8_t * buffer, int length);
		void SendAll(uint8_t * buffer, int length);
		virtual int SendTo(uint8_t * buffer, int length, const sockaddr * to, socklen_t tolength);
		virtual int ReceiveFrom(uint8_t * buffer, int length, const sockaddr * from, socklen_t fromlength);
	};
}