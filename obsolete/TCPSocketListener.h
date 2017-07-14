#pragma once
#include "TCPSocket.h"
#include <cstdint>
#include <memory>
#include <functional>
#include <thread>

namespace Net
{
	class TCPSocketListener
	{
	private:
		void OnConnection(std::shared_ptr<TCPSocket> socket);
		std::function<void(std::shared_ptr<TCPSocket>)> _onconnection;
		virtual std::shared_ptr<TCPSocket> Accept();
		std::shared_ptr<std::thread> listener;
		bool cancel;
		int clients;
	protected:
		intptr_t socket;
	public:
		TCPSocketListener();
		virtual ~TCPSocketListener();
		virtual std::shared_ptr<std::thread> & Listen(in6_addr address = in6addr_any, int port = 80);
		void Cancel();
		void SetConnectionHandler(std::function<void(std::shared_ptr<TCPSocket>)> onconnection);
	};
}
