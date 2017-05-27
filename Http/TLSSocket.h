#pragma once
#include "Socket.h"
#include <openssl/ssl.h>

namespace Net
{
	class TLSSocket : Socket
	{
	private:
		SSL * ssl;
	public:
		TLSSocket(SSL_CTX * sslctx, intptr_t socket);
		~TLSSocket();
		int Receive(uint8_t * buffer, int length) override;
		int Send(uint8_t * buffer, int length) override;
	};
}