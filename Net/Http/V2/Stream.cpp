#include "Stream.h"
#include "Frame.h"

using namespace Net::Http::V2;

Stream::Stream(uint32_t identifier, uint32_t initialwindowsize)
{
	this->identifier = identifier;
	this->rwindowsize = initialwindowsize;
	this->hwindowsize = initialwindowsize;
	exclusive = false;
	weight = 16;
	state = State::idle;
}

void Stream::Reset(Error::Code code)
{
	// Frame frame;
	// frame.type = Frame::Type::RST_STREAM;
	// frame.stream = shared_from_this();
	// frame.flags = Frame::Flag(0);
	// frame.length = 4;
	throw std::runtime_error("Not Implemented");
}

void Stream::ReceiveData(int length, bool endstream) {
	if(hwindowsize > window.size()) window.resize(hwindowsize);
	hwindowsize -= length;
}