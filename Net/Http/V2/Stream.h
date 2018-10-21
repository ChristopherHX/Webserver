#pragma once

#include "../Request.h"
#include "Frame.h"
#include "ErrorCode.h"
#include <cstdint>
#include <vector>
#include <functional>
#include <condition_variable>

namespace Net
{
	namespace Http
	{
		namespace V2
		{
			class Stream : std::enable_shared_from_this<Stream>
			{
			private:
				std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> _ondata;
				std::function<void(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> _oncontinuation;
				std::condition_variable cond_var;
				std::condition_variable data;
				std::condition_variable continuation;
				std::vector<uint8_t> window;
				std::vector<uint8_t>::const_iterator windowbeg;
				std::vector<uint8_t>::const_iterator windowend;
			public:
				enum class State : uint8_t
				{
					idle = 0b0,
					reserved_local = 0b0100,
					reserved_remote = 0b1000,
					open = 0b11,
					half_closed_local = 0b10,
					half_closed_remote = 0b01,
					closed = 0b10000
				};
				Stream(uint32_t identifier, uint32_t initialwindowsize);
				uint32_t identifier;
				State state;
				std::shared_ptr<Stream> dependency;
				std::vector<std::shared_ptr<Stream>> children;
				bool exclusive;
				uint8_t weight;
				uint32_t rwindowsize;
				uint32_t hwindowsize;
				void Reset(Error::Code code);
				void ReceiveData(int length, bool endstream);
				// void SendData(const uint8_t * buffer, int length, bool endstream);
				template <class Iter>
				Iter ParsePriority(Iter pos, const Session & session) {
					exclusive = *pos & 0x80;
					uint32_t dependency;
					pos = GetUInt31(pos, dependency);
					this->dependency = session->GetStream(dependency);
					if(exclusive) {
						children = this->dependency->children;
						this->dependency->children = {  };
					}
					weight = *pos++;
				}
			};
		}
	}
}