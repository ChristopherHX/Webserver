#pragma once
#include <string>

struct Request
{
	std::string method;
	std::string path;
	std::string query;
};