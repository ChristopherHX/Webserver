#include "ResponseImpl.h"
#include "../Response.h"

using namespace Net::Http::V1;

void ResponseImpl::Encode(const Header * header, std::vector<uint8_t>::iterator & buffer) const
{
	std::string statusline = "HTTP/1.1 " + std::to_string(((Response*)header)->status) + "\r\n";
	buffer = std::copy(statusline.begin(), statusline.end(), buffer);
	HeaderImpl::Encode(header, buffer);
}