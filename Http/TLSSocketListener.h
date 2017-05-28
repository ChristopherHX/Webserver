#pragma once
#include "SocketListener.h";
#include <memory>
#include <openssl/ssl.h>
#include <experimental/filesystem>

namespace Net
{
	class TLSSocketListener : public SocketListener
	{
	private:
		SSL_CTX * sslctx;
		std::experimental::filesystem::path privatekey;
		std::experimental::filesystem::path publiccertificate;
		std::shared_ptr<Socket> Accept() override;
	public:
		TLSSocketListener();
		~TLSSocketListener();
		void SetPrivateKey(const std::experimental::filesystem::path & privatekey);
		void SetPublicChainfile(const std::experimental::filesystem::path & publiccertificate);
		std::shared_ptr<std::thread> & Listen(IN6_ADDR address = in6addr_any, int port = 443) override;
	};
}