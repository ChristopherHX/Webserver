#include "Socket.h"
#include <stdexcept>

using namespace Net;

Socket::Socket(intptr_t socket, const in6_addr &address, int port)
{
	this->socket = socket;
	this->address = address;
	this->port = port;
}

Socket::~Socket()
{
	if (socket != -1)
	{
		shutdown(socket, 2);
		closesocket(socket);
		socket = -1;
	}
}

intptr_t Socket::GetSocket()
{
	return socket;
}

const IN6_ADDR & Socket::GetAddress()
{
	return address;
}

int Socket::GetPort()
{
	return port;
}

void Socket::SetProtocol(const std::string & protocol)
{
	this->protocol = protocol;
}

std::string Net::Socket::GetProtocol()
{
	return protocol;
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
