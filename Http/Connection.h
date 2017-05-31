#pragma once
#include "Socket.h"
#include "Request.h"
#include "Response.h"
#include <memory>

class Connection
{
public:
	std::shared_ptr<Net::Socket> socket;
	std::shared_ptr<Request> request;
	Response response;
	virtual void SendResponse() = 0;
	virtual void SendData(uint8_t * buffer, int length, bool last) = 0;
};

class Http1Connection : public Connection
{
public:
	void SendResponse() override;
	void SendData(uint8_t * buffer, int length, bool last) override;
};

class Http2Connection : public Connection
{
public:
	std::shared_ptr<HPack::Encoder> encoder;
	Frame frame;
	void SendResponse() override;
	void SendData(uint8_t * buffer, int length, bool last) override;
};