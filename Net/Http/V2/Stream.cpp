#include "Stream.h"

using namespace Net::Http::V2;

Stream::Stream(uint32_t identifier)
{
	this->identifier = identifier;
	state = StreamState::idle;
}

void Stream::OnData(const uint8_t * buffer, uint32_t length)
{
	if (!_ondata)
		throw std::runtime_error("No Data Handler");
	_ondata(buffer, length);
}

void Stream::SetOnData(std::function<void(const uint8_t*buffer, uint32_t length)> ondata)
{
	_ondata = ondata;
}

void Stream::OnContinuation(Frame & frame, const uint8_t * buffer, uint32_t length)
{
	if (!_oncontinuation)
		throw std::runtime_error("No Continuation Handler");
	_oncontinuation(frame, buffer, length);
}

void Stream::SetOnContinuation(std::function<void(Frame & frame, const uint8_t*buffer, uint32_t length)> oncontinuation)
{
	_oncontinuation = oncontinuation;
}

StreamPriority::StreamPriority()
{
	exclusive = false;
	dependency = 0;
	weight = 16;
}
