#pragma once

#include "../Request.h"
#include "Frame.h"
#include "ErrorCode.h"
#include <cstdint>
#include <vector>
#include <functional>

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
			public:
				Stream(uint32_t identifier);
				uint32_t identifier;
				StreamState state;
				StreamPriority priority;
				void OnData(std::vector<uint8_t>::const_iterator & buffer, uint32_t length);
				void SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata);
				void OnContinuation(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length);
				void SetOnContinuation(std::function<void(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata);
				void ResetStream(ErrorCode code);
			};
		}
	}
}