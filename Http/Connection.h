#pragma once
#include "Socket.h"
#include "Stream.h"
#include "Request.h"
#include "Response.h"
#include <memory>

class Connection
{
public:
	std::shared_ptr<Net::Socket> socket;
	std::shared_ptr<Stream> stream;
	Request request;
	Response response;
	virtual void SendResponse(bool endstream = false) = 0;
	virtual void SendData(uint8_t * buffer, int length, bool endstream = false) = 0;
	virtual void SetOnData(std::function<void(const uint8_t * buffer, uint32_t length)> ondata) = 0;
};

class Http1Connection : public Connection
{
private:
	std::function<void(const uint8_t*buffer, uint32_t length)> _ondata;
public:
	void SendResponse(bool endstream = false) override;
	void SendData(uint8_t * buffer, int length, bool endstream = false) override;
	void OnData(const uint8_t * buffer, uint32_t length);
	void SetOnData(std::function<void(const uint8_t * buffer, uint32_t length)> ondata) override;
};

class Http2Connection : public Connection
{
public:
	std::shared_ptr<HPack::Encoder> encoder;
	Frame frame;
	void SendResponse(bool endstream = false) override;
	void SendData(uint8_t * buffer, int length, bool endstream = false) override;
	void SetOnData(std::function<void(const uint8_t * buffer, uint32_t length)> ondata) override;
};