#pragma once
#include "V2/HPack/Encoder.h"
#include <sstream>
#include <string>
#include <vector>

class Response
{
public:
	int status;
	std::vector<std::pair<std::string, std::string>> headerlist;
	std::string ToHttp1Upper(const std::string & source);
	std::string ToHttp1();
	std::vector<uint8_t> ToHttp2(Net::Http::V2::HPack::Encoder & encoder);
};