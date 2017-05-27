#include "TLSSocketListener.h"
#include "TLSSocket.h"

using namespace Net;
using namespace std::experimental::filesystem;

TLSSocketListener::TLSSocketListener()
{
	OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
	sslctx = SSL_CTX_new(TLS_server_method());
}

TLSSocketListener::~TLSSocketListener()
{
	SSL_CTX_free(sslctx);
	OPENSSL_cleanup();
}

void TLSSocketListener::SetPrivateKey(const path & privatekey)
{
	if (!is_regular_file(privatekey))
		throw std::runtime_error("Private Key not found");
	this->privatekey = privatekey;
}

void TLSSocketListener::SetPublicChainfile(const path & publiccertificate)
{
	if (!is_regular_file(publiccertificate))
		throw std::runtime_error("Public certificate not found"); 
	this->publiccertificate = publiccertificate;
}

void TLSSocketListener::Listen(IN6_ADDR address, int port)
{
	if (privatekey.empty())
		throw std::runtime_error("Use SetPrivateKey to select a Private key file");
	if (publiccertificate.empty())
		throw std::runtime_error("Use SetPrivateKey to select a Public certificate file");
	SSL_CTX_use_PrivateKey_file(sslctx, privatekey.u8string().data(), SSL_FILETYPE_PEM);
	SSL_CTX_use_certificate_chain_file(sslctx, publiccertificate.u8string().data());
	SSL_CTX_set_alpn_select_cb(sslctx, [](SSL * ssl, const unsigned char ** out, unsigned char * outlen, const unsigned char * in, unsigned int inlen, void * args) -> int
	{
		return SSL_select_next_proto((unsigned char **)out, outlen, (const unsigned char *)"\x2h2", 3, in, inlen) == OPENSSL_NPN_NEGOTIATED ? 0 : 1;
	}, nullptr);
	SocketListener::Listen(address, port);
}

std::shared_ptr<Socket> TLSSocketListener::Accept()
{
	return std::dynamic_pointer_cast<Socket,TLSSocket>(std::make_shared<TLSSocket>(TLSSocket(sslctx, accept(socket, nullptr, nullptr))));
}
