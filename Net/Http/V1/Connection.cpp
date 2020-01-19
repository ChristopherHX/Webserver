#include "Connection.h"

using namespace Net::Http::V1;

void Connection::SendResponse(bool endstream)
{
	std::vector<uint8_t> buffer(1 << 20);
	auto end = buffer.begin();
	GetResponse().Encode(end);
	auto os = socket->GetOutputStream();
	os.SendAll(buffer.data(), end - buffer.begin());
}

void Connection::SendData(const uint8_t * buffer, int length, bool endstream)
{
	auto os = socket->GetOutputStream();
	os.SendAll(buffer, length);
}