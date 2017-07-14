#pragma once
#include "SocketListener.h";
#include <cstdint>
#include <memory>
#include <openssl/ssl.h>
//#include <experimental/filesystem>

namespace Net
{
	class TLSSocketListener : public TCPSocketListener
	{
	private:
		SSL_CTX * sslctx;
		std::shared_ptr<TCPSocket> Accept() override;
	public:
		TLSSocketListener();
		~TLSSocketListener();
		void UsePrivateKey(const std::string & privatekey);
		void UsePrivateKey(const uint8_t * buffer, int length);
		void UseCertificate(const std::string & certificate);
		void UseCertificate(const uint8_t * buffer, int length);
		std::shared_ptr<std::thread> & Listen(in6_addr address = in6addr_any, int port = 443) override;
	};
}