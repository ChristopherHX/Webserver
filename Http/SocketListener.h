#pragma once
#include "Socket.h"
#include <stdint.h>
#include <memory>
#include <functional>
#include <thread>

namespace Net
{
	class SocketListener
	{
	private:
		void OnConnection(std::shared_ptr<Socket> socket);
		std::function<void(std::shared_ptr<Socket>)> _onconnection;
		virtual std::shared_ptr<Socket> Accept();
		std::thread listening;
		int clients;
	protected:
		intptr_t socket;
	public:
		SocketListener();
		virtual ~SocketListener();
		virtual void Listen(IN6_ADDR address = in6addr_any, int port = 80);
		void SetConnectionHandler(std::function<void(std::shared_ptr<Socket>)> onconnection);
	};
}
