#include "SocketListener.h"
#include <stdexcept>
#include <string>

using namespace Net;

bool SocketListener::OnConnection(std::shared_ptr<Socket> socket)
{
	if (!_onconnection || clients > 10)
		return false;
	clients++;
	std::thread([this, socket]() {
		_onconnection(socket);
		clients--;
	}).detach();
	return true;
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
	clients = 0;
}

SocketListener::~SocketListener()
{
	Cancel();
	if (listener->joinable())
	{
		listener->join();
	}
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

std::shared_ptr<std::thread> & SocketListener::Listen(in6_addr address, int port)
{
	if (socket == -1)
		return std::shared_ptr<std::thread>();
	{
		sockaddr_in6 saddress;
		memset(&saddress, 0, sizeof(sockaddr_in6));
		saddress.sin6_family = AF_INET6;
		saddress.sin6_addr = address;
		saddress.sin6_port = htons(port);
		if (::bind(socket, (sockaddr *)&saddress, sizeof(saddress)) == -1)
			return std::shared_ptr<std::thread>();
	}
	if (listen(socket, 10) == -1)
		return std::shared_ptr<std::thread>();
	cancel = false;
	return listener = std::make_shared<std::thread>([this]() {
		while (!cancel)
		{
			auto socket = Accept();
			if(socket)
				OnConnection(socket);
		}
	});
}

void SocketListener::Cancel()
{
	cancel = true;
}

std::shared_ptr<Socket> SocketListener::Accept()
{
	sockaddr_in6 addresse;
	socklen_t size = sizeof(addresse);
	int socket = accept(this->socket, (sockaddr*)&addresse, &size);
	if (socket == -1)
		return std::shared_ptr<Socket>();
	return std::make_shared<Socket>(socket, addresse.sin6_addr, ntohs(addresse.sin6_port));
}

void SocketListener::SetConnectionHandler(std::function<void(std::shared_ptr<Socket>)> onconnection)
{
	_onconnection = onconnection;
}
