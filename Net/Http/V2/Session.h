#pragma once
#include "../Session.h"
#include "Frame.h"
#include "Stream.h"
#include "Setting.h"
#include "../Response.h"
#include <stack>
#include <mutex>
#include <vector>

namespace Net {
	namespace Http {
		namespace V2 {
			class Session : public Net::Http::Session {
			private:
				std::mutex sync;
				std::condition_variable synccv;
				std::shared_ptr<HPack::Encoder> encoder = std::make_shared<HPack::Encoder>();
				std::shared_ptr<HPack::Decoder> decoder = std::make_shared<HPack::Decoder>();
				std::vector<uint32_t> settings = { 4096, 1, (uint32_t)-1, 0xffff, 0x2fff, 0x4000, (uint32_t)-1 };
				std::vector<std::shared_ptr<Stream>> rstreams;
				std::vector<std::shared_ptr<Stream>> hstreams;
				std::stack<std::shared_ptr<std::vector<uint8_t>>> buffers;
			public:
				Session(std::shared_ptr<Net::Socket> &socket) : Net::Http::Session(socket) {

				}
				void Start();
				std::function<void(std::shared_ptr<Session>, std::shared_ptr<Stream>, std::shared_ptr<Net::Http::Request>)> requesthandler;				
				std::shared_ptr<Net::Http::V2::Stream> GetStream(uint32_t streamidentifier);
				void SendFrame(std::shared_ptr<Stream> stream, const Frame & frame);
				void SendFrame(std::shared_ptr<Stream> stream, const Frame & frame, std::vector<uint8_t>::iterator & data);
				void SendResponse(std::shared_ptr<Stream> stream, const Net::Http::Response & response, bool endstream);
				void SendData(std::shared_ptr<Stream> stream, const uint8_t* buffer, int length, bool endstream);
				uint32_t & GetSetting(Setting setting);
				std::shared_ptr<HPack::Decoder> GetDecoder()
				{
					return decoder;
				}
				void ReleaseBuffer(const std::shared_ptr<std::vector<uint8_t>> & buffer) {
					buffers.push(buffer);
				}
			};
		}
	}
}