#pragma once
#include "Socket.h"
#include "Frame.h"
#include "HPackDecoder.h"
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

class Request
{
public:
	static Request ParseHttp1(const uint8_t * buffer, int length);
	void ParseUrl(const std::string &path);
	void AppendHttp2(HPack::Decoder & decoder, const uint8_t * buffer, int length);
	std::string method;
	std::string path;
	std::string query;
	uintmax_t length;
	std::vector<std::pair<std::string, std::string>> headerlist;
};