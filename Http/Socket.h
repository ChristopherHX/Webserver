#pragma once
#include <cstdint>
#include <string>
#include <array>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WS2tcpip.h>
#endif

namespace Net
{
	class Socket
	{
	protected:
		intptr_t socket;
		in6_addr address;
		int port;
		std::string protocol;
	public:
		Socket(intptr_t socket, const in6_addr &address, int port);
		virtual ~Socket();
		intptr_t GetSocket();
		const IN6_ADDR &GetAddress();
		int GetPort();
		void SetProtocol(const std::string & protocol);
		std::string GetProtocol();
		virtual int Receive(uint8_t * buffer, int length);
		void ReceiveAll(uint8_t * buffer, int length);
		virtual int Send(uint8_t * buffer, int length);
		void SendAll(uint8_t * buffer, int length);
		template<int l>
		void SendAll(std::array<uint8_t, l> buffer) {
			return SendAll(buffer.data(), l);
		}
	};
}