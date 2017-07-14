#include "TLSSocketListener.h"
#include "TLSSocket.h"
#include <openssl/err.h>

using namespace Net;
//using namespace std::experimental::filesystem;

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

void TLSSocketListener::UsePrivateKey(const std::string & privatekey)
{
	if (SSL_CTX_use_PrivateKey_file(sslctx, privatekey.data(), SSL_FILETYPE_PEM) != 1)
	{
		throw std::runtime_error(u8"Invalid Private Key");
	}
}

void TLSSocketListener::UsePrivateKey(const uint8_t * buffer, int length)
{
	BIO * bio = BIO_new_mem_buf(buffer, length);
	EVP_PKEY * key = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
	BIO_free(bio);
	if (key == nullptr)
		throw std::runtime_error(u8"Invalid Privatekey");
	if (SSL_CTX_use_PrivateKey(sslctx, key) != 1)
		throw std::runtime_error(u8"OpenSSL can't use Privatekey");
}

void TLSSocketListener::UseCertificate(const std::string & certificate)
{ 
	if (SSL_CTX_use_certificate_chain_file(sslctx, certificate.data()) != 1)
	{
		throw std::runtime_error("Invalid Public Certificate");
	}
}

void TLSSocketListener::UseCertificate(const uint8_t * buffer, int length)
{
	std::shared_ptr<std::runtime_error> ex;
	BIO * bio = BIO_new_mem_buf(buffer, length);
	X509 * key = PEM_read_bio_X509_AUX(bio, nullptr, nullptr, nullptr);
	if (key == nullptr)
	{
		ex = std::make_shared<std::runtime_error>(u8"PEM_read_bio_X509_AUX failed");
	}
	else
	{
		if (SSL_CTX_use_certificate(sslctx, key) != 1)
		{
			ex = std::make_shared<std::runtime_error>(u8"SSL_CTX_use_certificate failed");
		}
		else
		{
			if (SSL_CTX_clear_chain_certs(sslctx) != 1)
			{
				ex = std::make_shared<std::runtime_error>(u8"SSL_CTX_clear_chain_certs failed");
			}
			else
			{
				{
					X509 *ca;
					while ((ca = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)) != nullptr)
					{
						if (SSL_CTX_add0_chain_cert(sslctx, ca) != 1)
						{
							X509_free(ca);
							ex = std::make_shared<std::runtime_error>(u8"SSL_CTX_add0_chain_cert failed");
							break;
						}
					}
				}
				{
					auto err = ERR_peek_last_error();
					if (ERR_GET_LIB(err) == ERR_LIB_PEM
						&& ERR_GET_REASON(err) == PEM_R_NO_START_LINE)
						ERR_clear_error();
					else
						ex = std::make_shared<std::runtime_error>(u8"PEM Format Error");
				}
			}
		}
	}
	X509_free(key);
	BIO_free(bio);
	if (ex != nullptr)
		throw ex;
}

std::shared_ptr<std::thread> & TLSSocketListener::Listen(in6_addr address, int port)
{
	if (SSL_CTX_check_private_key(sslctx) != 1)
	{
		throw std::runtime_error("Invalid Private Key / Public Certificate Pair");
	}
	SSL_CTX_set_alpn_select_cb(sslctx, [](SSL * ssl, const unsigned char ** out, unsigned char * outlen, const unsigned char * in, unsigned int inlen, void * args) -> int
	{
		return SSL_select_next_proto((unsigned char **)out, outlen, (const unsigned char *)"\x2h2\bhttp/1.1", 12, in, inlen) == OPENSSL_NPN_NEGOTIATED ? 0 : 1;
	}, nullptr);
	return TCPSocketListener::Listen(address, port);
}

std::shared_ptr<TCPSocket> TLSSocketListener::Accept()
{
	sockaddr_in6 addresse;
	socklen_t size = sizeof(addresse);
	intptr_t socket = accept(this->socket, (sockaddr*)&addresse, &size);
	if (socket == -1)
		throw std::runtime_error("Accept failed");
	return std::make_shared<TLSTCPSocket>(sslctx, socket, addresse.sin6_addr, ntohs(addresse.sin6_port));
}
