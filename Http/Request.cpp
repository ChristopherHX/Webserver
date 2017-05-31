#include "Request.h"
#include "utility.h"
#include <sstream>

Request::Request()
{

}

Request Request::ParseHttp1(uint8_t * buffer, int length)
{
	Request request;
	uint8_t * ofl = buffer;
	uint8_t * ofr = (uint8_t *)memchr(ofl, ' ', length - (ofl - buffer));
	request.method = std::string(ofl, ofr);
	ofl = ofr + 1;
	ofr = (uint8_t *)memchr(ofl, ' ', length - (ofl - buffer));
	std::string path(ofl, ofr);
	ofl = ofr + 1;
	ofr = (uint8_t *)memchr(ofl, '\r', length - (ofl - buffer));
	request.ParseUrl(path);
	request.headerlist.push_back({ ":method", request.method });
	request.headerlist.push_back({ ":path", path });
	while (ofr + 2 < buffer + length)
	{
		ofl = ofr + 2;
		ofr = (uint8_t *)memchr(ofl, '\r', length - (ofl - buffer));
		uint8_t * sep = (uint8_t *)memchr(ofl, ':', length - (ofl - buffer));
		if (sep != nullptr && sep < ofr)
		{
			std::string key(ofl, sep);
			std::transform(key.begin(), key.end(), key.begin(), ::tolower);
			if (std::find_if(request.headerlist.begin(), request.headerlist.end(), [&key](const std::pair<std::string, std::string> & pair) {return pair.first == key; }) == request.headerlist.end())
				request.headerlist.push_back({ key, std::string(sep + 2, ofr) });
		}
	}
	return request;
}

Request Request::ParseHttp2(HPack::Decoder & decoder, uint8_t * buffer, int length)
{
	Request request;
	decoder.DecodeHeaderblock(buffer, buffer + length, request.headerlist);
	for (auto & entry : request.headerlist)
	{
		if (entry.first == ":method")
		{
			request.method = entry.second;
		}
		else if (entry.first == ":path")
		{
			request.ParseUrl(entry.second);
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
