#pragma once
#include "Socket.h"
#include <openssl/ssl.h>

namespace Net
{
	class TLSSocket : public Socket
	{
	private:
		SSL * ssl;
	public:
		TLSSocket(SSL_CTX * sslctx, intptr_t socket, const in6_addr &address, int port);
		~TLSSocket();
		int Receive(uint8_t * buffer, int length) override;
		int Send(uint8_t * buffer, int length) override;
	};
}