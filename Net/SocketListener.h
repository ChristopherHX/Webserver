#pragma once
#include "Socket.h"
#include <cstdint>
#include <memory>
#include <functional>
#include <thread>

namespace Net
{
	class SocketListener
	{
	private:
		bool OnConnection(std::shared_ptr<Socket> socket);
		std::function<void(std::shared_ptr<Socket>)> _onconnection;
		virtual std::shared_ptr<Socket> Accept();
		std::shared_ptr<std::thread> listener;
		bool cancel;
		int clients;
	protected:
		intptr_t socket;
	public:
		SocketListener();
		virtual ~SocketListener();
		virtual std::shared_ptr<std::thread> & Listen(in6_addr address = in6addr_any, int port = 80);
		void Cancel();
		void SetConnectionHandler(std::function<void(std::shared_ptr<Socket>)> onconnection);
	};
}
