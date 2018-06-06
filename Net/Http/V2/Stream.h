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
			class StreamPriority
			{
			public:
				StreamPriority();
				StreamPriority(std::vector<uint8_t>::const_iterator & pos);
				bool exclusive;
				uint32_t dependency;
				uint8_t weight;
			};

			enum class StreamState : uint8_t
			{
				idle = 0b0,
				reserved_local = 0b0100,
				reserved_remote = 0b1000,
				open = 0b11,
				half_closed_local = 0b10,
				half_closed_remote = 0b01,
				closed = 0b10000
			};

			class Stream
			{
			private:
				std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> _ondata;
				std::function<void(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> _oncontinuation;
				std::condition_variable cond_var;
			public:
				Stream(uint32_t identifier, uint32_t initialwindowsize);
				uint32_t identifier;
				StreamState state;
				StreamPriority priority;
				uint32_t rwindowsize;
				uint32_t hwindowsize;
				void OnData(std::vector<uint8_t>::const_iterator & buffer, uint32_t length);
				void SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata);
				void OnContinuation(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length);
				void SetOnContinuation(std::function<void(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata);
				void Reset(ErrorCode code);
				void SendData(const uint8_t * buffer, int length, bool endstream);
			};
		}
	}
}