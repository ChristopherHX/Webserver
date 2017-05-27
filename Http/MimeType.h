#pragma once
#include <string>
#include <unordered_map>

using namespace std;

static class MimeType
{
public:
	static string Get(const string & extention);
};