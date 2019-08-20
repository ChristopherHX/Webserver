#pragma once
#include "../Socket.h"
#include "Request.h"
#include "Response.h"
#include <memory>
#include <functional>

namespace Net
{
	namespace Http
	{
		class Connection
		{
		public:
			std::shared_ptr<Net::Socket> socket;
			virtual Request& GetRequest() = 0;
			virtual Response& GetResponse() = 0;
			virtual void SendResponse(bool endstream = false) = 0;
			virtual void SendData(std::vector<uint8_t> & buffer, int length, bool endstream = false) = 0;
		};
	}
}