#include "../../Net/TLSSocketListener.h"
#include "../../Net/Http/V2/Frame.h"
#include "../../Net/Http/V2/HPack/Encoder.h"
#include "../../Net/Http/V2/HPack/Decoder.h"
#include "../../Net/Http/V2/Setting.h"
#include "../../Net/Http/V2/Stream.h"
#include "../../Net/Http/V2/ErrorCode.h"
#include "../../Net/Http/V2/Connection.h"
#include "../../Net/Http/V1/Connection.h"
#include "../PHPSapi/PHPSapi.h"

#include <unordered_map>
#include <algorithm>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <cstdint>

#include <experimental/filesystem>
using namespace Net::Http;
using namespace std::experimental::filesystem;

void RequestHandler(std::shared_ptr<Connection> connection)
{
	auto & request = connection->request;
	auto & response = connection->response;
	std::vector<uint8_t> buffer;
	if (request.method == "GET")
	{
		if (request.path == "/status.html")
		{
			auto & socket = connection->socket;
			response.status = 200;
			response.headerlist.push_back({ "content-length", "23" });
			connection->SendResponse();
			connection->SendData((uint8_t*)"Http/1-2 Server Running", 23, true);
		}
		else
		{
			path filepath = L"D:\\Web" / request.path;
			if (is_regular_file(filepath))
			{
				if (filepath.extension() == ".php")
				{
					PHPSapi::requesthandler(connection);
				}
				else
				{
					uintmax_t filesize = file_size(filepath);
					response.status = 200;
					response.headerlist.push_back({ "content-length", std::to_string(filesize) });
					connection->SendResponse();
					{
						std::vector<uint8_t> buffer(10240);
						std::ifstream filestream(filepath, std::ios::binary);
						for (uintmax_t i = filesize; i > 0;)
						{
							int count = std::min((uintmax_t)buffer.size(), i);
							filestream.read((char*)buffer.data(), count);
							connection->SendData(buffer.data(), count, count == i);
							i -= count;
						}
					}
				}
			}
		}
	}
	else
	{
		if (request.path == "/upload")
		{
			connection->SetOnData([connection](std::vector<uint8_t>::const_iterator & buffer, uint32_t length) {
				std::cout << "-------------------------------\n" << std::string((char*)&buffer[0], length) << "-------------------------------\n";
				connection->request.contentlength -= length;
				if (connection->request.contentlength == 0)
				{
					connection->response.status = 200;
					connection->response.headerlist.push_back({ "content-length", "0" });
					connection->SendResponse();
				}
			});
		}
	}
}

int main(int argc, const char** argv)
{
	Net::TLSSocketListener listener;
	listener.SetConnectionHandler([](std::shared_ptr<Net::Socket> socket) {
		std::vector<uint8_t> buffer(65535);
		if (socket->GetProtocol() == "h2")
		{
			using namespace V2;
			try
			{
				std::shared_ptr<HPack::Encoder> encoder = std::make_shared<HPack::Encoder>();
				std::shared_ptr<HPack::Decoder> decoder = std::make_shared<HPack::Decoder>();
				std::vector<uint32_t> settings = { 4096, 1, (uint32_t)-1, 0xffff, 0x2fff, 0x4000, (uint32_t)-1 };
				std::vector<std::shared_ptr<Stream>> rstreams;
				std::vector<std::shared_ptr<Stream>> hstreams;
				socket->ReceiveAll(buffer.data(), 24);
				while (socket->ReceiveAll(buffer.data(), 9))
				{
					Frame frame(buffer.begin());
					if (frame.length > buffer.size())
					{
						return;
					}
					if (socket->ReceiveAll(buffer.data(), frame.length))
					{
						auto pos = buffer.begin(), end = pos + frame.length;
						uint8_t padlength = 0;
						if (frame.HasFlag(FrameFlag::PADDED))
						{
							padlength = *pos++;
							if (padlength > frame.length)
								throw ErrorCode::PROTOCOL_ERROR;
							end -= padlength;
						}
						switch (frame.type)
						{
						case FrameType::DATA:
						{
							if (frame.streamidentifier == 0)
								return;// throw ErrorCode::PROTOCOL_ERROR;
							if (frame.streamidentifier & 1 == 1 && !(rstreams[frame.streamidentifier >> 1]->state == StreamState::open || rstreams[frame.streamidentifier >> 1]->state == StreamState::half_closed_local))
								return;//throw ErrorCode::STREAM_CLOSED;
							if ((end - pos) > 0 && frame.streamidentifier & 1)
								rstreams[frame.streamidentifier >> 1]->OnData(pos, end - pos);
							break;
						}
						case FrameType::HEADERS:
						{
							if (frame.streamidentifier == 0)
								throw ErrorCode::PROTOCOL_ERROR;
							uint32_t id = frame.streamidentifier >> 1;
							if (rstreams.size() <= id)
							{
								rstreams.resize(id + 1);
								auto stream = rstreams[id] = std::make_shared<Stream>(frame.streamidentifier);
								stream->state = frame.HasFlag(FrameFlag::END_STREAM) ? StreamState::half_closed_remote : StreamState::open;
								if (frame.HasFlag(FrameFlag::PRIORITY))
								{
									stream->priority.exclusive = *pos & 0x80;
									stream->priority.dependency = GetUInt31(pos);
									stream->priority.weight = *pos++;
								}
								std::shared_ptr<Net::Http::V2::Connection> connection = std::make_shared<Net::Http::V2::Connection>();
								connection->request.DecodeHeaderblock(decoder, pos, end - pos);
								connection->encoder = encoder;
								connection->frame = frame;
								connection->socket = socket;
								connection->stream = stream;
								if (frame.HasFlag(FrameFlag::END_HEADERS))
								{
									RequestHandler(connection);
								}
								else
								{
									stream->SetOnContinuation([connection, &decoder](Frame & frame, std::vector<uint8_t>::const_iterator & buffer, uint32_t length) {
										connection->request.DecodeHeaderblock(decoder, buffer, length);
										if (frame.HasFlag(FrameFlag::END_HEADERS))
										{
											RequestHandler(connection);
										}
									});
								}
							}
							break;
						}
						case FrameType::PRIORITY:
						{
							/*
							rstreams[frame.streamidentifier >> 1]->priority.exclusive = *pos & 0x80;
							rstreams[frame.streamidentifier >> 1]->priority.dependency = GetUInt31(pos);
							rstreams[frame.streamidentifier >> 1]->priority.weight = *pos++;*/
							pos += 5;
							break;
						}
						case FrameType::RST_STREAM:
						{
							ErrorCode code = (ErrorCode)GetUInt32(pos);
							rstreams[frame.streamidentifier >> 1]->state = StreamState::closed;
							//Abort Work
							break;
						}
						case FrameType::SETTINGS:
						{
							if (!frame.HasFlag(FrameFlag::ACK))
							{
								while (pos != end)
								{
									uint16_t id = GetUInt16(pos) - 1;
									uint32_t value = GetUInt32(pos);
									settings[id] = value;
								}
								{
									Frame response;
									response.length = 0;
									response.type = FrameType::SETTINGS;
									response.flags = FrameFlag::ACK;
									response.streamidentifier = frame.streamidentifier;
									socket->SendAll(response.ToArray());
								}
							}
							break;
						}
						case FrameType::PING:
						{
							if (frame.flags != FrameFlag::ACK)
							{
								Frame response;
								response.length = frame.length;
								response.type = FrameType::PING;
								response.flags = FrameFlag::ACK;
								response.streamidentifier = frame.streamidentifier;
								socket->SendAll(response.ToArray());
								socket->SendAll(buffer.data(), response.length);
							}
							break;
						}
						case FrameType::GOAWAY:
						{
							uint32_t laststreamid = GetUInt32(pos);
							ErrorCode code = (ErrorCode)GetUInt32(pos);
							{
								Frame response;
								response.length = frame.length;
								response.type = FrameType::GOAWAY;
								response.flags = (FrameFlag)0;
								response.streamidentifier = frame.streamidentifier;
								socket->SendAll(response.ToArray());
								auto wpos = buffer.begin();
								AddUInt31(laststreamid, wpos);
								AddUInt32((uint32_t)ErrorCode::NO_ERROR, wpos);
								socket->SendAll(buffer.data(), response.length);
							}
							return;
						}
						case FrameType::WINDOW_UPDATE:
						{
							//uint32_t WindowSizeIncrement = ntohl(((*(uint32_t*)&*pos) << 1) & 0xfffffffe);
							//buffer.resize(buffer.size() + WindowSizeIncrement);
							break;
						}
						case FrameType::CONTINUATION:
							rstreams[frame.streamidentifier >> 1]->OnContinuation(frame, pos, end - pos);
							break;
						}
					}
					else
					{
						break;
					}
				}
			}
			catch (const std::runtime_error & error)
			{
				std::cout << error.what() << "\n";
			}
		}
		else
		{
			int content = 0;
			std::shared_ptr<V1::Connection> connection;
			while (true)
			{
				int count = socket->Receive(buffer.data(), content == 0 ? buffer.size() : std::min(content, (int)buffer.size()));
				if (count > 0)
				{
					if (content <= 0)
					{
						connection = std::make_shared<V1::Connection>();
						connection->request = Request::ParseHttp1(buffer.data(), count);
						connection->socket = socket;
						content = connection->request.contentlength;
						RequestHandler(connection);
					}
					else
					{
						content -= count;
						connection->OnData(buffer.begin(), count);
					}
				}
				else
				{
					break;
				}
			}
		}
	});
	listener.UsePrivateKey("D:\\Users\\administrator\\Documents\\privatekey.pem", Net::SSLFileType::PEM);
	listener.UseCertificate("D:\\Users\\administrator\\Documents\\certificate.cer", Net::SSLFileType::PEM);
	auto address = std::make_shared<sockaddr_in6>();
	memset(address.get(), 0, sizeof(sockaddr_in6));
	address->sin6_family = AF_INET6;
	address->sin6_port = htons(443);
	address->sin6_addr = in6addr_any;
	listener.AddProtocol("h2");
	PHPSapi::init();
	listener.Listen(std::shared_ptr<sockaddr>(address, (sockaddr*)address.get()), sizeof(sockaddr_in6))->join();
	PHPSapi::deinit();
	return 0;
}