#include "TLSSocketListener.h"
#include "TLSSocket.h"
#include <openssl/err.h>

using namespace Net;

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

bool TLSSocketListener::UsePrivateKey(const std::string & privatekey, SSLFileType ftype)
{
	return SSL_CTX_use_PrivateKey_file(sslctx, privatekey.data(), (int)ftype);
}

bool TLSSocketListener::UsePrivateKey(const uint8_t * buffer, int length, SSLFileType ftype)
{
	BIO * bio = BIO_new_mem_buf(buffer, length);
	EVP_PKEY * pkey;
	switch(ftype)
	{
		case SSLFileType::PEM:
			pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
			break;
		case SSLFileType::DER:
			pkey = d2i_PrivateKey_bio(bio, 0);
			break;
	}
	BIO_free(bio);
	if(pkey != nullptr)
		SSL_CTX_use_PrivateKey(sslctx, pkey);
	return false;
}

bool TLSSocketListener::UseCertificate(const std::string & certificate, SSLFileType ftype)
{ 
	return ftype != SSLFileType::PEM ? SSL_CTX_use_certificate_file(sslctx, certificate.data(), (int)ftype) : SSL_CTX_use_certificate_chain_file(sslctx, certificate.data());
}

bool TLSSocketListener::UseCertificate(const uint8_t * buffer, int length, SSLFileType ftype)
{
	switch (ftype)
	{
	case Net::SSLFileType::PEM:
	{
		BIO * bio = BIO_new_mem_buf(buffer, length);
		X509 * key = PEM_read_bio_X509_AUX(bio, nullptr, nullptr, nullptr);
		bool ret;
		if (ret = key != nullptr)
		{
			if (ret = SSL_CTX_use_certificate(sslctx, key))
			{
				if (ret = SSL_CTX_clear_chain_certs(sslctx))
				{
					{
						X509 *ca;
						while ((ca = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)) != nullptr)
						{
							if (SSL_CTX_add0_chain_cert(sslctx, ca))
							{
								X509_free(ca);
								ret = false;
								break;
							}
						}
					}
					{
						auto err = ERR_peek_last_error();
						if (ERR_GET_LIB(err) == ERR_LIB_PEM	&& ERR_GET_REASON(err) == PEM_R_NO_START_LINE)
							ERR_clear_error();
						else
							ret = false;
					}
				}
			}
		}
		X509_free(key);
		BIO_free(bio);
		return ret;
	}
	case Net::SSLFileType::DER:
		return false;
	default:
		return false;
	}
}

std::shared_ptr<std::thread> & TLSSocketListener::Listen(in6_addr address, int port)
{
	if (SSL_CTX_check_private_key(sslctx) != 1)
		return std::shared_ptr<std::thread>();
	SSL_CTX_set_alpn_select_cb(sslctx, [](SSL * ssl, const unsigned char ** out, unsigned char * outlen, const unsigned char * in, unsigned int inlen, void * args) -> int
	{
		return SSL_select_next_proto((unsigned char **)out, outlen, (const unsigned char *)"\x2h2\bhttp/1.1", 12, in, inlen) == OPENSSL_NPN_NEGOTIATED ? 0 : 1;
	}, nullptr);
	return SocketListener::Listen(address, port);
}

std::shared_ptr<Socket> TLSSocketListener::Accept()
{
	sockaddr_in6 addresse;
	socklen_t size = sizeof(addresse);
	intptr_t socket = accept(this->socket, (sockaddr*)&addresse, &size);
	if (socket == -1)
		return std::shared_ptr<Socket>();
	return std::make_shared<TLSSocket>(sslctx, socket, addresse.sin6_addr, ntohs(addresse.sin6_port));
}
