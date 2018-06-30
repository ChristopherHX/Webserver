#include "Stream.h"
#include "Frame.h"

using namespace Net::Http::V2;

Stream::Priority::Priority()
{
	exclusive = false;
	dependency = 0;
	weight = 16;
}

Stream::Stream(uint32_t identifier, uint32_t initialwindowsize)
{
	this->identifier = identifier;
	this->rwindowsize = initialwindowsize;
	this->hwindowsize = initialwindowsize;
	state = State::idle;
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

void Stream::Reset(Error::Code code)
{
	Frame frame;
	frame.type = Frame::Type::RST_STREAM;
	frame.stream = shared_from_this();
	frame.flags = Frame::Flag(0);
	frame.length = 4;

}

void Stream::ReceiveData(int length, bool endstream) {
	if(hwindowsize > window.size()) window.resize(hwindowsize);
	hwindowsize -= length;
}

void Stream::SendData(const uint8_t* buffer, int length, bool endstream) {
	Frame frame;
	frame.type = Frame::Type::DATA;
	frame.flags = (Frame::Flag)0;
	frame.stream = shared_from_this();
	if (length > 0) {
		for (int i = 0; i < length; i += frame.length) {
			frame.length = std::min(512, length - i);
			if (endstream && frame.length != 512)
				frame.flags = Frame::Flag::END_STREAM;
			//socket->SendAll(result.ToArray());
			//socket->SendAll(buffer + i, result.length);
		}
	}
	else {
		if (endstream)
			frame.flags = Frame::Flag::END_STREAM;
		frame.length = 0;
		//socket->SendAll(result.ToArray());
	}
}