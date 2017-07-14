#include "TCPSocket.h"
#include <stdexcept>

using namespace Net;

TCPSocket::TCPSocket(intptr_t socket, const in6_addr &address, int port)
{
	this->socket = socket;
	this->address = address;
	this->port = port;
}

TCPSocket::~TCPSocket()
{
	if (socket != -1)
	{
		shutdown(socket, 2);
		closesocket(socket);
		socket = -1;
	}
}

intptr_t TCPSocket::GetSocket()
{
	return socket;
}

const in6_addr & TCPSocket::GetAddress()
{
	return address;
}

int TCPSocket::GetPort()
{
	return port;
}

void TCPSocket::SetProtocol(const std::string & protocol)
{
	this->protocol = protocol;
}

std::string Net::TCPSocket::GetProtocol()
{
	return protocol;
}

int TCPSocket::Receive(uint8_t * buffer, int length)
{
	return recv(socket, (char*)buffer, length, 0);
}

void TCPSocket::ReceiveAll(uint8_t * buffer, int length)
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

int TCPSocket::Send(uint8_t * buffer, int length)
{
	return send(socket, (char*)buffer, length, 0);
}

void TCPSocket::SendAll(uint8_t * buffer, int length)
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

int Net::TCPSocket::SendTo(uint8_t * buffer, int length, const sockaddr * to, socklen_t tolength)
{
	return sendto(socket, buffer, length, 0, to, tolength);
}

int Net::TCPSocket::ReceiveFrom(uint8_t * buffer, int length, const sockaddr * from, socklen_t fromlength)
{
	group_req d;
	return recvfrom(socket, buffer, length, 0, from, &fromlength);
}
