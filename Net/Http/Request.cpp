#include "Request.h"
#include "../../utility.h"
#include <sstream>
#include <algorithm>

using namespace Net::Http;

Request Request::ParseHttp1(const uint8_t * buffer, int length)
{
	Request request;
	const uint8_t * ofl = buffer;
	const uint8_t * ofr = (const uint8_t *)memchr(ofl, ' ', length - (ofl - buffer));
	request.method = std::string(ofl, ofr);
	ofl = ofr + 1;
	ofr = (const uint8_t *)memchr(ofl, ' ', length - (ofl - buffer));
	std::string path(ofl, ofr);
	ofl = ofr + 1;
	ofr = (const uint8_t *)memchr(ofl, '\r', length - (ofl - buffer));
	request.ParseUrl(path);
	request.headerlist.push_back({ ":method", request.method });
	request.headerlist.push_back({ ":path", path });
	while (ofr + 2 < buffer + length)
	{
		ofl = ofr + 2;
		ofr = (const uint8_t *)memchr(ofl, '\r', length - (ofl - buffer));
		const uint8_t * sep = (const uint8_t *)memchr(ofl, ':', length - (ofl - buffer));
		if (sep != nullptr && sep < ofr)
		{
			std::string key(ofl, sep);
			std::transform(key.begin(), key.end(), key.begin(), ::tolower);
			if (std::find_if(request.headerlist.begin(), request.headerlist.end(), [&key](const std::pair<std::string, std::string> & pair) {return pair.first == key; }) == request.headerlist.end())
				request.headerlist.push_back({ key, std::string(sep + 2, ofr) });
		}
	}
	for (auto & entry : request.headerlist)
	{
		if (entry.first == "content-length")
		{
			request.length = std::stoll(entry.second);
		}
	}
	return request;
}

void Request::ParseUrl(const std::string & path)
{
	size_t sep = path.find('?');
	if (sep != std::string::npos)
	{
		query = path.substr(sep + 1);
		std::transform(query.begin(), query.end(), query.begin(), [](char ch) {return ch == '+' ? ' ' : ch; });
		query = Utility::urlDecode(query);
		this->path = Utility::urlDecode(path.substr(0, sep));
	}
	else
	{
		this->path = Utility::urlDecode(path);
	}
}

void Request::AppendHttp2(Net::Http::V2::HPack::Decoder & decoder, const uint8_t * buffer, int length)
{
	decoder.DecodeHeaderblock(buffer, buffer + length, headerlist);
	for (auto & entry : headerlist)
	{
		if (entry.first == ":method")
		{
			method = entry.second;
		}
		else if (entry.first == ":path")
		{
			ParseUrl(entry.second);
		}
		else if (entry.first == "content-length")
		{
			this->length = std::stoll(entry.second);
		}
	}
}
