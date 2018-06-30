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
				class Priority
				{
				public:
					Priority();
					template <class Iter>
					static Iter Parse(Iter pos, Priority & priority) {
						exclusive = *pos & 0x80;
						dependency = GetUInt31(pos);
						weight = *pos++;
					}
					bool exclusive;
					uint32_t dependency;
					uint8_t weight;
				};
				Stream(uint32_t identifier, uint32_t initialwindowsize);
				uint32_t identifier;
				State state;
				Priority priority;
				uint32_t rwindowsize;
				uint32_t hwindowsize;
				void OnData(std::vector<uint8_t>::const_iterator & buffer, uint32_t length);
				void SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata);
				void Reset(Error::Code code);
				void ReceiveData(int length, bool endstream);
				void SendData(const uint8_t * buffer, int length, bool endstream);
			};
		}
	}
}