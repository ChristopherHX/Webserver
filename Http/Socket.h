#pragma once
#include <stdint.h>
namespace Net
{
	class Socket
	{
	protected:
		intptr_t socket;
	public:
		Socket(intptr_t socket);
		virtual ~Socket();
		intptr_t GetSocket();
		virtual int Receive(uint8_t * buffer, int length);
		void ReceiveAll(uint8_t * buffer, int length);
		virtual int Send(uint8_t * buffer, int length);
		void SendAll(uint8_t * buffer, int length);
	};
}