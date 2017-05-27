#include "Socket.h"
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WS2tcpip.h>
#endif

using namespace Net;

Socket::Socket(intptr_t socket)
{
	this->socket = socket;
}

Socket::~Socket()
{
	shutdown(socket, 2);
	closesocket(socket);
	socket = -1;
}

intptr_t Socket::GetSocket()
{
	return socket;
}

int Socket::Receive(uint8_t * buffer, int length)
{
	return recv(socket, (char*)buffer, length, 0);
}

void Socket::ReceiveAll(uint8_t * buffer, int length)
{
	uint8_t * end = buffer + length;
	while (buffer < end)
	{
		int received = Receive(buffer, end - buffer);
		if (received <= 0)
		{
			throw std::runtime_error(u8"Abored because of Socket-Error");
		}
		buffer += received;
	}
}

int Socket::Send(uint8_t * buffer, int length)
{
	return send(socket, (char*)buffer, length, 0);
}

void Socket::SendAll(uint8_t * buffer, int length)
{
	uint8_t * end = buffer + length;
	while (buffer < end)
	{
		int sent = Send(buffer, end - buffer);
		if (sent <= 0)
		{
			throw std::runtime_error(u8"Abored because of Socket-Error");
		}
		buffer += sent;
	}
}
