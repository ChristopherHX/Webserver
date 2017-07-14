#include "TLSSocket.h"
#include <vector>

using namespace Net;

TLSSocket::TLSSocket(SSL_CTX * sslctx, intptr_t socket, const in6_addr &address, int port) : Socket(socket, address, port)
{
	ssl = SSL_new(sslctx);
	SSL_set_fd(ssl, socket);
	SSL_accept(ssl);
	const unsigned char * alpn;
	unsigned int len;
	SSL_get0_alpn_selected(ssl, &alpn, &len);
	protocol = std::string((const char*)alpn, len);
}

TLSSocket::~TLSSocket()
{
	SSL_shutdown(ssl);
	SSL_free(ssl);
}

int TLSSocket::Receive(uint8_t * buffer, int length)
{
	return SSL_read(ssl, buffer, length);
}

int TLSSocket::Send(uint8_t * buffer, int length)
{
	return SSL_write(ssl, buffer, length);
}
