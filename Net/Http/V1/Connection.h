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
            public:
                void SendResponse(bool endstream = false) override;
                void SendData(const uint8_t * buffer, int length, bool endstream = false) override;
            };
        }
    }
}