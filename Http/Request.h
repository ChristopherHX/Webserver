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
	Request();
	static Request ParseHttp1(const std::shared_ptr<Net::Socket> &socket, uint8_t * buffer, int length);
	static Request ParseHttp2(const std::shared_ptr<Frame> &frame, const std::shared_ptr<Net::Socket> &socket, HPack::Decoder & Decoder, uint8_t * buffer, int length);
	void ParseUrl(const std::string &path);
	std::shared_ptr<Frame> frame;
	std::shared_ptr<Net::Socket> socket;
	std::string method;
	std::string path;
	std::string query;
	std::vector<std::pair<std::string, std::string>> headerlist;
};