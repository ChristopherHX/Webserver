#include "Response.h"

using namespace Net::Http;

size_t hstatus = Net::Http::Header::hash_fn(":status");

Net::Http::Response::Response() : status(200)
{
}

bool Net::Http::Response::Add(size_t hash, const std::pair<std::string, std::string>& pair)
{
	if (hash == hstatus)
	{
		status = std::stol(pair.second);
	}
	else
	{
		return Header::Add(hash, pair);
	}
	return true;
}

void Response::EncodeHttp1(std::vector<uint8_t>::iterator & buffer) const
{
	std::string statusline = "HTTP/1.1 " + std::to_string(status) + "\r\n";
	buffer = std::copy(statusline.begin(), statusline.end(), buffer);
	Header::EncodeHttp1(buffer);
}

void Response::EncodeHttp2(std::shared_ptr<V2::HPack::Encoder> encoder, std::vector<uint8_t>::iterator &buffer) const
{
	std::vector<std::pair<std::string, std::string>> header = { { ":status", std::to_string(status) } };
	encoder->AddHeaderBlock(buffer, header);
	Header::EncodeHttp2(encoder, buffer);
}