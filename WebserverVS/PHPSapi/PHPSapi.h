#pragma once
#include "../../../Net/Http/Connection.h"
namespace PHPSapi
{
	void init();
	void deinit();
	void requesthandler(std::shared_ptr<Net::Http::Connection> connection);
}