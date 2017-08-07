#pragma once
#include "../Connection.h"

namespace Net
{
	namespace Http
	{
        namespace V1
        {
            class Connection : public Net::Http::Connection
            {
            private:
                std::function<void(const uint8_t*buffer, uint32_t length)> _ondata;
            public:
				//Connection(std::shared_ptr<Net::Socket> socket, Request request);

                void SendResponse(bool endstream = false) override;
                void SendData(uint8_t * buffer, int length, bool endstream = false) override;
                void OnData(const uint8_t * buffer, uint32_t length);
                void SetOnData(std::function<void(const uint8_t * buffer, uint32_t length)> ondata) override;
            };
        }
    }
}