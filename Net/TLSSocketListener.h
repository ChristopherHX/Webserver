#pragma once
#include "SocketListener.h"
#include <cstdint>
#include <memory>
#include <openssl/ssl.h>

namespace Net
{
	enum class SSLFileType
	{
		PEM = SSL_FILETYPE_PEM,
		DER = X509_FILETYPE_ASN1
	};

	class TLSSocketListener : public SocketListener
	{
	private:
		SSL_CTX * sslctx;
		std::shared_ptr<Socket> Accept() override;
	public:
		TLSSocketListener();
		~TLSSocketListener();
		bool UsePrivateKey(const std::string & privatekey, SSLFileType ftype);
		bool UsePrivateKey(const uint8_t * buffer, int length, SSLFileType ftype);
		bool UseCertificate(const std::string & certificate, SSLFileType ftype);
		bool UseCertificate(const uint8_t * buffer, int length, SSLFileType ftype);
		std::shared_ptr<std::thread> & Listen(in6_addr address = in6addr_any, int port = 443) override;
	};
}