#include "Http.h"
#include "../../../utility.h"
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#undef max
#define SHUT_RDWR SD_BOTH
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <sys/signalfd.h>
#include <sys/signal.h>
#include <dlfcn.h>
#define closesocket(socket) close(socket)
#endif

using namespace Http::V1;

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
			this->values[values.substr(i, (lineend == std::string::npos ? values.length() : lineend) - i)] = "";
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
#ifdef _WIN32
	std::ostringstream ss;
#else
	std::string res;
#endif
	for (auto &pair : values)
	{
#ifdef _WIN32
		ss << pair.first;
		if (pair.second != "")
		{
			ss << "=" << pair.second;
		}
		if ((++values.find(pair.first)) != values.end())
		{
			ss << "; ";
		}
#else
		std::ostringstream ss;
		// ss.seekp(0, std::ios::beg);
		if ((++values.find(pair.first)) != values.end())
		{
			ss << "; ";
		}
		ss << pair.first;
		if (pair.second != "")
		{
			ss << "=" << pair.second;
		}
		res = ss.str() + res;
#endif
	}
#ifdef _WIN32
	return ss.str();
#else
	return res;
#endif
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
			if (parameter.find(key) == parameter.end())
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
	std::ostringstream ss;
	for (auto &pair : parameter)
	{
		ss << pair.first << ": " << pair.second.toString() << "\r\n";
	}
	return ss.str();
}

Request::Request(std::string headerstr) : Parameter(headerstr.substr(headerstr.find("\r\n") + 2))
{
	std::istringstream iss(headerstr);
	std::getline(iss, methode, ' ');
	std::getline(iss, request, ' ');
	iss.ignore(5, '/');
	iss >> version;
	size_t pq = request.find('?');
	if (pq != std::string::npos)
	{
		putValues = Utility::UrlDecode(String::Replace(request.substr(pq + 1), "+", " "));
		request = Utility::UrlDecode(request.substr(0, pq));
	}
	else
	{
		request = Utility::UrlDecode(request);
	}
}

std::string Request::toString()
{
	return methode + " " + Utility::UrlEncode(request + (putValues.toString() == "" ? "" : ("?" + putValues.toString()))) + " HTTP/1.1\r\n" + Parameter::toString() + "\r\n";
}

Response::Response()
{
}

Response::Response(std::string headerstr) : Parameter(headerstr.substr(headerstr.find("\r\n") + 2))
{
	std::istringstream iss(headerstr);
	iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
	std::getline(iss, status, '\r');
}

std::string Response::toString()
{
	return "HTTP/1.1 " + status + "\r\n" + Parameter::toString() + "\r\n";
}