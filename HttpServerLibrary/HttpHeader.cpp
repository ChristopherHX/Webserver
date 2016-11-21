#include "HttpHeader.h"
#include "ByteArray.h"
#include "utility.h"
#include <sstream>
#include <algorithm>

using namespace Http;

std::string &String::Replace(std::string &&str, const char * search, const char * replace)
{
	for (int pos = str.find(search); pos < str.length() && pos != std::string::npos; pos = str.find(search, pos))
	{
		str = str.replace(pos, strlen(search), replace);
		pos += strlen(replace);
	}
	return str;
}

Values::Values()
{
}

Values::Values(const std::string &values)
{
	int i = 0;
	while (i != std::string::npos)
	{
		unsigned int lineend = values.find("; ", i);
		unsigned int dpI = values.find("=", i);
		if (dpI < lineend)
		{
			std::string key = values.substr(i, dpI - i);
			if (this->values.find(key) == this->values.end())
				this->values[key] = values.substr(dpI + 1, (lineend == std::string::npos ? values.length() : lineend) - (dpI + 1));
		}
		else
		{
			this->values[values.substr(i, (lineend == std::string::npos ? values.length() : lineend) - i)];
		}
		i = std::max(lineend, lineend + 2);
	}
}

Values::Values(const char * values) : Values(std::string(values))
{

}

std::string & Values::operator[](const std::string & key)
{
	return values[key];
}

bool Values::Exists(std::string key)
{
	return values.find(key) != values.end();
}

std::string Values::toString()
{
	std::stringstream ss;
	for (auto &pair : values)
	{
		ss << pair.first;
		if (pair.second != "")
		{
			ss << "=" << pair.second;
		}
		if ((++values.find(pair.first)) != values.end())
		{
			ss << "; ";
		}
	}
	return ss.str();
}


Parameter::Parameter()
{
}

Parameter::Parameter(std::string paramstr)
{
	int i = 0;
	while (i != std::string::npos)
	{
		unsigned int lineend = paramstr.find("\r\n", i);
		unsigned int dpI = paramstr.find(": ", i);
		if (dpI < lineend)
		{
			std::string key = paramstr.substr(i, dpI - i);
			if(parameter.find(key) == parameter.end())
				parameter[key] = paramstr.substr(dpI + 2, (lineend == std::string::npos ? paramstr.length() : lineend) - (dpI + 2));
		}
		i = std::max(lineend, lineend + 2);
	}
}

Values & Parameter::operator[](const std::string &key)
{
	return parameter[key];
}

bool Parameter::Exists(std::string key)
{
	return parameter.find(key) != parameter.end();
}

void Parameter::Clear()
{
	parameter.clear();
}

std::string Parameter::toString()
{
	std::stringstream ss;
	for (auto &pair : parameter)
	{
		ss << pair.first << ": " << pair.second.toString() << "\r\n";
	}
	return ss.str();
}

Request::Request()
{
}

Request::Request(std::string headerstr) : Parameter(headerstr.substr(headerstr.find("\r\n") + 2))
{
	int patha = headerstr.find(" ");
	methode = headerstr.substr(0, patha);
	int pathb = headerstr.find(" ", patha + 1);
	request = headerstr.substr(patha + 1, pathb - (patha + 1));
	int putI = request.find("?");
	putValues = putI == std::string::npos ? "" : Utility::urlDecode(request.substr(putI + 1));
	request = Utility::urlDecode(putI == std::string::npos ? request : request.substr(0, putI));
	//ByteArray buf0(5), buf1(256), buf2(256);
	//sscanf(headerstr.data(), "%4s %255[^]? ]c?%255[^]??%255s HTTP/%L\r\n", buf0.Data(), buf1.Data(), buf2.Data(), &Version);
	//request = UriDecode(buf1);
	//putValues = UriDecode(buf2);
}

std::string Request::toString()
{
	return methode + " " + String::Replace(request + (putValues.toString() == "" ? "" : ("?" + putValues.toString())), " ", "%20") + " HTTP/1.1\r\n" + Parameter::toString() + "\r\n";
}

Response::Response()
{
}

Response::Response(std::string headerstr) : Parameter(headerstr.substr(headerstr.find("\r\n") + 2))
{
	int spaceI = headerstr.find(" ");
	status = headerstr.substr(spaceI, headerstr.find("\r\n") - spaceI);
}

std::string Response::toString()
{
	return "HTTP/1.1 " + status + "\r\n" + Parameter::toString() + "\r\n";
}