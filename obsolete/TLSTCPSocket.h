#pragma once
#include "TCPSocket.h"
#include <openssl/ssl.h>

namespace Net
{
	class TLSTCPSocket : public TCPSocket
	{
	private:
		SSL * ssl;
	public:
		TLSTCPSocket(SSL_CTX * sslctx, intptr_t socket, const in6_addr &address, int port);
		~TLSTCPSocket();
		int Receive(uint8_t * buffer, int length) override;
		int Send(uint8_t * buffer, int length) override;
	};
}