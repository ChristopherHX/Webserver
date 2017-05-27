#include "SocketListener.h"
#include <stdexcept>

using namespace Net;

void SocketListener::OnConnection(std::shared_ptr<Socket> socket)
{
	if (!_onconnection)
		throw std::runtime_error("You have to set an Connectionhandler");
	if (clients > 10)
		throw std::runtime_error("Too many Connections");
	clients++;
	std::thread([this, socket]() {
		_onconnection(socket);
		clients--;
	}).detach();
}

SocketListener::SocketListener()
{
#ifdef _WIN32
	WSADATA data;
	WSAStartup(WINSOCK_VERSION, &data);
#endif // _WIN32
	if((socket = ::socket(AF_INET6, SOCK_STREAM, 0)) != -1)
	{
		uint32_t value = 0;
		setsockopt(socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&value, sizeof(value));
		value = 1;
		setsockopt(socket, IPPROTO_TCP, TCP_FASTOPEN, (const char*)&value, sizeof(value));
		setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&value, sizeof(value));
	}
}

SocketListener::~SocketListener()
{
	if (socket != -1)
	{
		shutdown(socket, 2);
		closesocket(socket);
		socket = -1;
	}
#ifdef _WIN32
	WSACleanup();
#endif // _WIN32
}

void SocketListener::Listen(IN6_ADDR address, int port)
{
	if (socket == -1)
		throw std::runtime_error("Invalid Socket");
	{
		sockaddr_in6 saddress;
		memset(&saddress, 0, sizeof(sockaddr_in6));
		saddress.sin6_family = AF_INET6;
		saddress.sin6_addr = address;
		saddress.sin6_port = htons(port);
		if (::bind(socket, (sockaddr *)&saddress, sizeof(saddress)) == -1)
		{
			int error = WSAGetLastError();
			throw std::runtime_error(u8"Can't bind Socket");
		}
	}
	if (listen(socket, 10) == -1)
	{
		throw std::runtime_error(u8"Can't listen Socket");
	}
	//listening = std::thread([this]() {
		while (true)
		{
			auto socket = Accept();
			OnConnection(socket);
		}
	//});
}

std::shared_ptr<Socket> SocketListener::Accept()
{
	intptr_t res = accept(socket, nullptr, nullptr);
	return std::make_shared<Socket>(res);
}

void SocketListener::SetConnectionHandler(std::function<void(std::shared_ptr<Socket>)> onconnection)
{
	_onconnection = onconnection;
}
