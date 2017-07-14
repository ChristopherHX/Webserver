#include "Response.h"

std::string Response::ToHttp1Upper(const std::string & source)
{
	std::string result(source);
	bool upper = true;
	for (auto & ch : result)
	{
		if (upper)
		{
			ch = toupper(ch);
		}
		upper = !isalpha(ch);
	}
	return result;
}

std::string Response::ToHttp1()
{
	std::ostringstream result;
	result << "HTTP/1.1 " << std::to_string(status) << "\r\n";
	for (auto &entry : headerlist)
	{
		std::string key;
		if (entry.first.at(0) == ':')
		{
			if (entry.first == ":autority")
			{
				key = "Host";
			}
			else
			{
				continue;
			}
		}
		else
		{
			key = ToHttp1Upper(entry.first);
		}
		result << key << ": " << entry.second << "\r\n";
	}
	result << "\r\n";
	return result.str();
}

std::vector<uint8_t> Response::ToHttp2(HPack::Encoder & encoder)
{
	std::vector<uint8_t> result;
	encoder.AppendHeaderBlock(result, { { ":status", std::to_string(status) } });
	encoder.AppendHeaderBlock(result, headerlist);
	return result;
}
