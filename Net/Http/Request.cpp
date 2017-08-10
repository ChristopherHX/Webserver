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
	request.ParseUri(path);
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
			request.contentlength = std::stoll(entry.second);
		}
	}
	return request;
}

void Request::ParseUri(const std::string & uri)
{
	size_t sep = uri.find('?');
	if (sep != std::string::npos)
	{
		query = uri.substr(sep + 1);
		std::transform(query.begin(), query.end(), query.begin(), [](char ch) {return ch == '+' ? ' ' : ch; });
		query = Utility::UrlDecode(query);
		path = Utility::UrlDecode(uri.substr(0, sep));
		this->uri = path + '?' + query;
	}
	else
	{
		this->uri = path = Utility::UrlDecode(uri);
	}
}

void Net::Http::Request::DecodeHeaderblock(std::shared_ptr<V2::HPack::Decoder>& decoder, std::vector<uint8_t>::const_iterator & buffer, int length)
{
	decoder->DecodeHeaderblock(buffer, length, headerlist);
	for (auto & entry : headerlist)
	{
		if (entry.first == ":method")
		{
			method = entry.second;
		}
		else if (entry.first == ":path")
		{
			ParseUri(entry.second);
		}
		else if (entry.first == "content-length")
		{
			this->contentlength = std::stoll(entry.second);
		}
		else if (entry.first == "content-type")
		{
			this->contenttype = entry.second;
		}
	}
}