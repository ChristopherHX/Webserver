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

void Stream::SendFrame(std::shared_ptr<Stream> stream, const Frame &frame)
{
	auto os = socket->GetOutputStream();
	os.SendAll(frame.ToArray());
}

void Stream::SendFrame(std::shared_ptr<Stream> stream, const Frame &frame, std::vector<uint8_t>::iterator & data)
{
	auto os = socket->GetOutputStream();
	os.SendAll(frame.ToArray());
	os.SendAll(&*data, frame.length);
}

void Net::Http::V2::Session::SendResponse(std::shared_ptr<Stream> stream, const Response & response, bool endstream)
{
	std::vector<uint8_t> buffer(1 << 20);
	auto end = buffer.begin();
	response.EncodeHttp2(encoder, end);
	Frame result;
	result.length = end - buffer.begin();
	result.type = Frame::Type::HEADERS;
	result.flags = Frame::Flag::END_HEADERS;
	if(endstream) 
		(uint8_t&)result.flags |= (uint8_t)Frame::Flag::END_STREAM;
	result.stream = stream;
        auto pos = buffer.begin();
	SendFrame(stream, result, pos);
}

void Net::Http::V2::Session::SendData(std::shared_ptr<Stream> stream, const uint8_t * buffer, int length, bool endstream)
{
	Frame result;
	result.type = Frame::Type::DATA;
	result.flags = (Frame::Flag)0;
	result.stream = stream;
	uint32_t chunk = settings[Setting::MAX_FRAME_SIZE];
	do
	{
		result.length = std::min<uint32_t>(chunk, length);
		if (endstream && result.length == length)
			result.flags = Frame::Flag::END_STREAM;
		{
			auto os = socket->GetOutputStream();
			os.SendAll(result.ToArray());
			os.SendAll(buffer, result.length);
		}
		length -= result.length;
		buffer += result.length;
	} while (length > 0);
}

void Stream::ReceiveData(int length, bool endstream) {
	if(hwindowsize > window.size()) window.resize(hwindowsize);
	hwindowsize -= length;
}