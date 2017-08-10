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
			Request request;
			Response response;
			virtual void SendResponse(bool endstream = false) = 0;
			void SendData(std::vector<uint8_t> & buffer, int length, bool endstream = false);
			virtual void SendData(const uint8_t * buffer, int length, bool endstream = false) = 0;
			virtual void SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata) = 0;
		};
	}
}