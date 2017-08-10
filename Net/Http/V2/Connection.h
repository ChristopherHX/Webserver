#include "../Connection.h"
#include "Stream.h"
#include "HPack/Encoder.h"

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			class Connection : public Net::Http::Connection
			{
			public:
				std::shared_ptr<HPack::Encoder> encoder;
				Frame frame;
				std::shared_ptr<Stream> stream;
				void SendResponse(bool endstream = false) override;
				void SendData(const uint8_t * buffer, int length, bool endstream = false) override;
				void SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata) override;
			};
		}
    }
}