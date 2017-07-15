#pragma once

#include "../Request.h"
#include "Frame.h"
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
				idle,
				reserved_local,
				reserved_remote,
				open,
				half_closed_local,
				half_closed_remote,
				closed
			};

			class Stream
			{
			private:
				std::function<void(const uint8_t * buffer, uint32_t length)> _ondata;
				std::function<void(Frame & frame, const uint8_t * buffer, uint32_t length)> _oncontinuation;
			public:
				Stream(uint32_t identifier);
				uint32_t identifier;
				StreamState state;
				StreamPriority priority;
				void OnData(const uint8_t * buffer, uint32_t length);
				void SetOnData(std::function<void(const uint8_t * buffer, uint32_t length)> ondata);
				void OnContinuation(Frame & frame, const uint8_t * buffer, uint32_t length);
				void SetOnContinuation(std::function<void(Frame & frame, const uint8_t * buffer, uint32_t length)> ondata);
			};
		}
	}
}