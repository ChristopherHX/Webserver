#pragma once
#include "../Session.h"
#include "Setting.h"
#include "../Request.h"
#include "../Response.h"
#include <stack>
#include <mutex>
#include <vector>
#include <functional>
#include "HPack/Decoder.h"
#include "HPack/Encoder.h"

namespace Net {
	namespace Http {
		namespace V2 {
			class Stream;
			template<class Iter>
			Iter GetUInt16(Iter buffer, uint16_t & number)
			{
				number = *buffer << 8 | *(buffer + 1);
				return buffer + 2;
			}

			template<class Iter>
			Iter GetUInt24(Iter buffer, uint32_t & number)
			{
				number = *buffer << 16 | *(buffer + 1) << 8 | *(buffer + 2);
				return buffer + 3;
			}

			template<class Iter>
			Iter GetUInt32(Iter buffer, uint32_t & number)
			{
				number = (*buffer << 24) | (*(buffer + 1) << 16) | (*(buffer + 2) << 8) | *(buffer + 3);
				return buffer + 4;
			}

			template<class Iter>
			Iter GetUInt31(Iter buffer, uint32_t & number)
			{
				auto pos = GetUInt32(buffer, number);
				number &= 0x7fffffff;
				return pos;
			}

			class Session : public Net::Http::Session {
			private:
				std::mutex sync;
				std::condition_variable synccv;
				std::shared_ptr<HPack::Encoder> encoder = std::make_shared<HPack::Encoder>();
				std::shared_ptr<HPack::Decoder> decoder = std::make_shared<HPack::Decoder>();
				std::vector<std::shared_ptr<Stream>> streams;
			public:
				std::unordered_map<Setting,uint32_t> settings = { {Setting::HEADER_TABLE_SIZE, 4096}, {Setting::ENABLE_PUSH, 1}, { Setting::MAX_CONCURRENT_STREAMS, -1}, { Setting::INITIAL_WINDOW_SIZE, 0xffff}, { Setting::MAX_FRAME_SIZE, 0x2fff}, {Setting::MAX_HEADER_LIST_SIZE, 0x4000}};
				Session(std::shared_ptr<Net::Socket> &socket) : Net::Http::Session(socket) {

				}
				void Start();
				std::function<void(std::shared_ptr<Session>, std::shared_ptr<Stream>, std::shared_ptr<Net::Http::Request>)> requesthandler;
				std::shared_ptr<Stream> GetStream(uint32_t id) const;
				std::shared_ptr<HPack::Decoder> GetDecoder()
				{
					return decoder;
				}
				std::shared_ptr<HPack::Encoder> GetEncoder()
				{
					return encoder;
				}
			};
		}
	}
}

#include "Stream.h"