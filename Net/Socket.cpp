#include "Socket.h"
#include <stdexcept>

using namespace Net;

Socket::Socket(SOCKET socket, const in6_addr &address, int port)
{
	this->handle = socket;
	this->address = address;
	this->port = port;
}

Socket::Socket(SOCKET socket, const sockaddr_storage & address)
{

}

Socket::Socket()
{
	
	//struct in_addr interface_addr;
	//int s = sizeof(interface_addr);
	//getsockopt(handle, IPPROTO_IP, IP_MULTICAST_IF, (char*)&interface_addr, &s);
	//addrinfo addr;
	//memset(&addr, 0, sizeof(addr));
	//addr.ai_family = AF_UNSPEC;
	//addr.ai_socktype = SOCK_DGRAM;
	//addrinfo * result;
	
	//getaddrinfo("127.0.0.1", nullptr, &addr, &result);
	//freeaddrinfo(result);
}

Net::Socket::Socket(Socket && socket)
{
	handle = socket.handle;
	socket.handle = -1;
}

Socket::~Socket()
{
	if (handle != -1)
	{
		shutdown(handle, 2);
		closesocket(handle);
		handle = -1;
	}
}

Socket Socket::Accept(SOCKET listener)
{
	SOCKADDR_STORAGE addresse;
	socklen_t size = sizeof(addresse);
	SOCKET socket = accept(listener, (sockaddr*)&addresse, &size);
	if (socket == -1)
		throw std::runtime_error("Accept failed");
	return Socket(socket, addresse);
}

SOCKET Socket::GetSocket()
{
	return handle;
}

const in6_addr & Socket::GetAddress()
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
	return recv(handle, (char*)buffer, length, 0);
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
	return send(handle, (char*)buffer, length, 0);
}

int Socket::Send(std::vector<uint8_t> buffer)
{
	return Send(buffer.data(), buffer.size());
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

void Socket::SendAll(std::vector<uint8_t> buffer)
{
	SendAll(buffer.data(), buffer.size());
}

int Net::Socket::SendTo(uint8_t * buffer, int length, const sockaddr * to, socklen_t tolength)
{
	return sendto(handle, (char*)buffer, length, 0, to, tolength);
}

int Net::Socket::ReceiveFrom(uint8_t * buffer, int length, sockaddr * from, socklen_t * fromlength)
{
	group_req d;
	return recvfrom(handle, (char*)buffer, length, 0, from, fromlength);
}
