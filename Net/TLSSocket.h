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
		TLSSocket(SSL_CTX * sslctx, intptr_t socket, const std::shared_ptr<sockaddr> &socketaddress);
		TLSSocket(SSL_CTX * sslctx, const std::shared_ptr<Socket>& socket);
		~TLSSocket();
		int Receive(uint8_t * buffer, int length) override;
		int Send(uint8_t * buffer, int length) override;
	};
}