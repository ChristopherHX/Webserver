#include "Stream.h"
#include "Frame.h"

using namespace Net::Http::V2;

Stream::Stream(uint32_t identifier, uint32_t initialwindowsize)
{
	this->identifier = identifier;
	this->rwindowsize = initialwindowsize;
	this->hwindowsize = initialwindowsize;
	state = StreamState::idle;
}

void Stream::OnData(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)
{
	if (!_ondata) {
		std::mutex m;
		std::unique_lock<std::mutex> lock(m);
		cond_var.wait(lock);
	}
		//throw std::runtime_error("No Data Handler");
	_ondata(buffer, length);
}

void Stream::SetOnData(std::function<void(std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> ondata)
{
	_ondata = ondata;
	cond_var.notify_all();
}

void Stream::OnContinuation(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length)
{
	if (!_oncontinuation)
		throw std::runtime_error("No Continuation Handler");
	_oncontinuation(frame, buffer, length);
}

void Stream::SetOnContinuation(std::function<void(Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length)> oncontinuation)
{
	_oncontinuation = oncontinuation;
}

void Stream::Reset(ErrorCode code)
{
}

void Stream::SendData(const uint8_t* buffer, int length, bool endstream)
{
	Frame frame;
	frame.type = FrameType::DATA;
	frame.flags = (FrameFlag)0;
	frame.streamidentifier = identifier;
	if (length > 0)
	{
		for (int i = 0; i < length; i += frame.length)
		{
			frame.length = std::min(512, length - i);
			if (endstream && frame.length != 512)
				frame.flags = FrameFlag::END_STREAM;
			//socket->SendAll(result.ToArray());
			//socket->SendAll(buffer + i, result.length);
		}
	}
	else
	{
		if (endstream)
			frame.flags = FrameFlag::END_STREAM;
		frame.length = 0;
		//socket->SendAll(result.ToArray());
	}
}

StreamPriority::StreamPriority()
{
	exclusive = false;
	dependency = 0;
	weight = 16;
}

Net::Http::V2::StreamPriority::StreamPriority(std::vector<uint8_t>::const_iterator & pos)
{
	exclusive = *pos & 0x80;
	dependency = GetUInt31(pos);
	weight = *pos++;
}
