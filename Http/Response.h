#pragma once
#include <sstream>
#include <string>
#include <vector>

class Response
{
public:
	int status;
	std::vector<std::pair<std::string, std::string>> headerlist;
	std::string ToHttp1Upper(const std::string & source)
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
	}

	std::string ToHttp1()
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

	std::vector<uint8_t> ToHttp2()
	{
		std::vector<uint8_t> result;

		return result;
	}
};