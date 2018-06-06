#include "Session.h"
#include "Setting.h"
#include "../Request.h"
#include "../Response.h"
#include <fstream>
#include <thread>
#include <experimental/filesystem>

using namespace Net::Http::V2;
using namespace std::experimental::filesystem;

static std::unordered_map<FrameType, std::function<void(std::shared_ptr<Session>, std::shared_ptr<std::vector<uint8_t>>, std::shared_ptr<Frame>)>> framehandler = {
	{ FrameType::DATA, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
	if (frame->streamidentifier == 0)
		throw ErrorCode::PROTOCOL_ERROR;
	auto stream = session->GetStream(frame->streamidentifier);
	if (!(stream->state == StreamState::open || stream->state == StreamState::half_closed_local))
		throw ErrorCode::STREAM_CLOSED;
	auto beg = buffer->cbegin();
	auto end = beg + frame->length;
	if (frame->HasFlag(FrameFlag::PADDED))
	{
		uint8_t padlength = *beg++;
		if (padlength > frame->length)
			throw ErrorCode::PROTOCOL_ERROR;
		end -= padlength;
	}
	stream->OnData(beg, end - beg);
} },
{ FrameType::HEADERS, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
	if (frame->streamidentifier == 0)
		throw ErrorCode::PROTOCOL_ERROR;
	auto stream = session->GetStream(frame->streamidentifier);
	stream->state = frame->HasFlag(FrameFlag::END_STREAM) ? StreamState::half_closed_remote : StreamState::open;
	auto beg = buffer->cbegin();
	auto end = beg + frame->length;	
	if (frame->HasFlag(FrameFlag::PRIORITY))//???Wie war es jetzt
	{
		stream->priority = StreamPriority(beg);
	}
	if (frame->HasFlag(FrameFlag::PADDED))
	{
		uint8_t padlength = *beg++;
		if (padlength > frame->length)
			throw ErrorCode::PROTOCOL_ERROR;
		end -= padlength;
	}
	std::shared_ptr<Net::Http::Request> request = std::make_shared<Net::Http::Request>();
	request->DecodeHttp2(session->GetDecoder(), beg, end);
	if (frame->HasFlag(FrameFlag::END_HEADERS))
	{
		session->requesthandler(session, stream, request);
	}
	//else
	//{
	//	stream->SetOnContinuation([connection, &decoder](Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length) {
	//		connection->request.DecodeHttp2(decoder, buffer, buffer + length);
	//		if (frame.HasFlag(FrameFlag::END_HEADERS))
	//		{
	//			RequestHandler(connection);
	//		}
	//	});
	//}
} },{ FrameType::PRIORITY, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
        auto pos = buffer->cbegin();
	session->GetStream(frame->streamidentifier)->priority = StreamPriority(pos);
} },{ FrameType::RST_STREAM, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
	auto pos = buffer->cbegin();
        session->GetStream(frame->streamidentifier)->Reset((ErrorCode)GetUInt32(pos));
} },{ FrameType::SETTINGS, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
	if (!frame->HasFlag(FrameFlag::ACK))
	{
		auto pos = buffer->cbegin(), end = pos + frame->length;
		while (pos != end)
		{
			Setting s = (Setting)(GetUInt16(pos) - 1);
			session->GetSetting(s) = GetUInt32(pos);
		}
		{
			Frame response;
			response.length = 0;
			response.type = FrameType::SETTINGS;
			response.flags = FrameFlag::ACK;
			response.streamidentifier = frame->streamidentifier;
			session->SendFrame(session->GetStream(frame->streamidentifier), response);
		}
	}
} },{ FrameType::PING, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
	if (!frame->HasFlag(FrameFlag::ACK))
	{
		Frame response;
		response.length = frame->length;
		response.type = FrameType::PING;
		response.flags = FrameFlag::ACK;
		response.streamidentifier = frame->streamidentifier;
                auto pos = buffer->begin();
		session->SendFrame(session->GetStream(frame->streamidentifier), response, pos);
	}
} },{ FrameType::GOAWAY, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
	auto pos = buffer->cbegin();
	uint32_t laststreamid = GetUInt31(pos);
	ErrorCode code = (ErrorCode)GetUInt32(pos);
	{
		Frame response;
		response.length = frame->length;
		response.type = FrameType::GOAWAY;
		response.flags = (FrameFlag)0;
		response.streamidentifier = frame->streamidentifier;
		auto wpos = buffer->begin();
		AddUInt31(laststreamid, wpos);
		AddUInt32((uint32_t)ErrorCode::NO_ERROR, wpos);
                auto pos = buffer->begin();
		session->SendFrame(session->GetStream(frame->streamidentifier), response, pos);
	}
} },{ FrameType::WINDOW_UPDATE, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
        auto pos = buffer->cbegin();
	session->GetStream(frame->streamidentifier)->rwindowsize += GetUInt31(pos);
	//buffer.resize(buffer.size() + WindowSizeIncrement);
} },{ FrameType::CONTINUATION, [](std::shared_ptr<Session> session, std::shared_ptr<std::vector<uint8_t>> buffer, std::shared_ptr<Frame> frame) {
        auto pos = buffer->cbegin();
	session->GetStream(frame->streamidentifier)->OnContinuation(*frame, pos, frame->length);
} }
};

void Net::Http::V2::Session::Start()
{
	{
		uint8_t buffer[24];
		if (!socket->ReceiveAll(buffer, 24) || memcmp(buffer, "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n", 24))
			throw std::runtime_error(u8"Invalid Connection Preface");
	}
	while (true)
	{
		std::shared_ptr<std::vector<uint8_t>> buffer;
		if (buffers.empty())
		{
			buffer = std::make_shared<std::vector<uint8_t>>(settings[(size_t)Setting::MAX_FRAME_SIZE]);
		}
		else
		{
			buffer = buffers.top();
			buffers.pop();
		}
		if (socket->ReceiveAll(buffer->data(), 9))
		{
                        auto pos = buffer->cbegin();
			auto frame = std::make_shared<Frame>(pos);
			if (frame->length < settings[(size_t)Setting::MAX_FRAME_SIZE])
			{
				if (buffer->size() < frame->length)
					buffer->resize(settings[(size_t)Setting::MAX_FRAME_SIZE]);
				if (socket->ReceiveAll(buffer->data(), frame->length))
				{
					std::thread run([](std::shared_ptr<Session> session, std::shared_ptr<Frame> frame, std::shared_ptr<std::vector<uint8_t>> buffer) {
						try {
							framehandler.at(frame->type)(session,buffer, frame);
						}
						catch (const std::runtime_error & error) {
							//std::cout << "frame error: " << error.what() << std::endl;
							//__debugbreak();
						}
						session->ReleaseBuffer(buffer);
					}, std::static_pointer_cast<Session>(shared_from_this()), frame, buffer);
					run.detach();
				}
			}
			else
			{
				break;
			}
		}
	}
}

std::shared_ptr<Net::Http::V2::Stream> Net::Http::V2::Session::GetStream(uint32_t streamidentifier)
{
	uint32_t id = streamidentifier >> 1;
	if ((streamidentifier & 1) == 1)
	{
		if (rstreams.size() <= id)
			rstreams.resize(id + 1);
		return rstreams[id] ? rstreams[id] : (rstreams[id] = std::make_shared<Net::Http::V2::Stream>(streamidentifier, settings[(size_t)Setting::INITIAL_WINDOW_SIZE]));
	}
	else
	{
		if (hstreams.size() <= id)
			hstreams.resize(id + 1);
		return hstreams[id] ? hstreams[id] : (hstreams[id] = std::make_shared<Net::Http::V2::Stream>(streamidentifier, settings[(size_t)Setting::INITIAL_WINDOW_SIZE]));
	}
}

void Net::Http::V2::Session::SendFrame(std::shared_ptr<Stream> stream, const Frame &frame)
{
	if (stream->priority.dependency)
	{
		std::unique_lock<std::mutex> lock(sync);		
		auto wait_fn = [stream = GetStream(stream->priority.dependency)]() -> bool {
			return stream->state == StreamState::closed;
		};
		if(!wait_fn())
			synccv.wait(lock, wait_fn);
	}
	socket->SendAll(frame.ToArray());
}

void Net::Http::V2::Session::SendFrame(std::shared_ptr<Stream> stream, const Frame &frame, std::vector<uint8_t>::iterator & data)
{
	if (stream->priority.dependency)
	{
		std::unique_lock<std::mutex> lock(sync);		
		auto wait_fn = [stream = GetStream(stream->priority.dependency)]() -> bool {
			return stream->state == StreamState::closed;
		};
		if(!wait_fn())
			synccv.wait(lock, wait_fn);
	}
	socket->SendAll(frame.ToArray());
	socket->SendAll(&*data, frame.length);
}

void Net::Http::V2::Session::SendResponse(std::shared_ptr<Stream> stream, const Response & response, bool endstream)
{
	std::vector<uint8_t> buffer(1 << 20);
	auto end = buffer.begin();
	response.EncodeHttp2(encoder, end);
	Frame result;
	result.length = end - buffer.begin();
	result.type = FrameType::HEADERS;
	result.flags = FrameFlag::END_HEADERS;
	if(endstream) 
		(uint8_t&)result.flags |= (uint8_t)FrameFlag::END_STREAM;
	result.streamidentifier = stream->identifier;
        auto pos = buffer.begin();
	SendFrame(stream, result, pos);
}

void Net::Http::V2::Session::SendData(std::shared_ptr<Stream> stream, const uint8_t * buffer, int length, bool endstream)
{
	Frame result;
	result.type = FrameType::DATA;
	result.flags = (FrameFlag)0;
	result.streamidentifier = stream->identifier;
	uint32_t chunk = settings[(uint32_t)Setting::MAX_FRAME_SIZE];
	do
	{
		result.length = std::min<uint32_t>(chunk, length);
		if (endstream && result.length == length)
			result.flags = FrameFlag::END_STREAM;
		socket->SendAll(result.ToArray());
		socket->SendAll(buffer, result.length);
		length -= result.length;
		buffer += result.length;
	} while (length > 0);
}

uint32_t & Net::Http::V2::Session::GetSetting(Setting setting)
{
	return settings[(size_t)setting];
}
