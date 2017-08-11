#include "Request.h"
#include "../../utility.h"
#include <sstream>
#include <algorithm>

using namespace Net::Http;

size_t hmethod = Net::Http::Header::hash_fn(":method");
size_t hpath = Net::Http::Header::hash_fn(":path");

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

bool Net::Http::Request::Add(size_t hash, const std::pair<std::string, std::string>& pair)
{
	if (hash == hmethod)
	{
		method = pair.second;
	}
	else if (hash == hpath)
	{
		ParseUri(pair.second);
	}
	else
	{
		return false;
	}
	return true;
}

void Net::Http::Request::DecodeHttp1(std::vector<uint8_t>::const_iterator & buffer, const std::vector<uint8_t>::const_iterator & end)
{
	std::vector<uint8_t>::const_iterator ofr = std::find(buffer, end, ' ');
	method = std::string(buffer, ofr);
	buffer = ofr + 1;
	ofr = std::find(buffer, end, ' ');
	ParseUri(std::string(buffer, ofr));
	buffer = ofr + 1;
	const char rn[] = "\r\n";
	buffer = std::search(buffer, end, rn, std::end(rn)) + 2;
	Header::DecodeHttp1(buffer, end);
}
