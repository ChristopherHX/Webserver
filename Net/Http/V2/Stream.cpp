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

// void Stream::SendData(const uint8_t* buffer, int length, bool endstream) {
// 	Frame frame;
// 	frame.type = Frame::Type::DATA;
// 	frame.flags = (Frame::Flag)0;
// 	frame.stream = shared_from_this();
// 	if (length > 0) {
// 		for (int i = 0; i < length; i += frame.length) {
// 			frame.length = std::min(512, length - i);
// 			if (endstream && frame.length != 512)
// 				frame.flags = Frame::Flag::END_STREAM;
// 			//socket->SendAll(result.ToArray());
// 			//socket->SendAll(buffer + i, result.length);
// 		}
// 	}
// 	else {
// 		if (endstream)
// 			frame.flags = Frame::Flag::END_STREAM;
// 		frame.length = 0;
// 		//socket->SendAll(result.ToArray());
// 	}
// }