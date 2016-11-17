#include "HttpHeader.h"
#include "HttpRequestBuffer.h"
#include "HttpServer.h"
#include "ByteArray.h"
#include <sstream>
#include <experimental/filesystem>
#include <list>
#include <fstream>
#include <iostream>

using namespace Http;
namespace fs = std::experimental::filesystem;

static const std::string htmltop("<!DOCTYPE html>\
<html>\
<head>\
    <title>Datei Explorer</title>\
    <link rel=\"stylesheet\" href=\"/css/folder.css\" type=\"text/css\" />\
    <script src=\"/js/folder.js\" type=\"text/javascript\"></script>\
</head>\
<body>\
    <div id=\"header\">\
        <div>\
            <input type=\"file\" id=\"filebox\" multiple=\"multiple\" onchange=\"fileChange();\" />\
            <button type=\"button\" onclick=\"uploadfiles();\">Hochladen</button>\
            <button type=\"button\" onclick=\"uploadabort();\">Abbrechen</button>\
        </div>\
        <div>\
            <progress id=\"progressbar\"></progress>\
            <span id=\"status\"></span>\
        </div>\
    </div>");

static const std::string htmlbottom("</body>\
</html>");

bool compare_nocase(const std::string& first, const std::string& second)
{
	unsigned int i = 0;
	while ((i<first.length()) && (i<second.length()))
	{
		if (tolower(first[i])<tolower(second[i])) return true;
		else if (tolower(first[i])>tolower(second[i])) return false;
		++i;
	}
	return (first.length() < second.length());
}

std::map<std::string, std::string> benutzer;

int seite(RequestBuffer& buffer)
{
	auto& requestHeader = buffer.Request();
	if (requestHeader.httpMethode == "GET" || requestHeader.httpMethode == "HEAD")
	{
		std::string serverPath(ParameterValues(String::Replace(std::move(requestHeader.putPath), "%2F", "/"))["folder"]);
		fs::path folderPath = buffer.RootPath() / serverPath;
		if (!fs::is_directory(folderPath))
		{
			throw NotFoundException("Ordner existiert nicht");
		}
		std::list<std::string> folder;
		std::list<std::string> files;
		for (fs::directory_entry entry : fs::directory_iterator(folderPath))
		{
			std::string name(entry.path().filename().u8string());
			fs::file_status status(entry.status());
			switch (status.type())
			{
			case fs::file_type::regular:
				files.push_back(name);
				break;
			case fs::file_type::directory:
				folder.push_back(name);
				break;
			default:
				break;
			}
		}
		std::stringstream contentstream;
		contentstream << htmltop;
		if (benutzer.size() > 0)
		{
			contentstream << "<div>Letzte Anmeldung am Server als" << (benutzer.begin())->second << "</div><br/>";
		}
		contentstream << "<div id=\"path\">" << serverPath << "</div>";
		if (serverPath != "/") contentstream << "<div><a class=\"button button3\" href=\"/libordner.so?folder=" << serverPath.substr(0, serverPath.find_last_of('/', serverPath.length() - 2)) << "/\" target=\"_self\">..</button></div>";
		folder.sort(compare_nocase);
		for (auto &name : folder)
		{
			contentstream << "<div><a class=\"button button3\" href=\"/libordner.so?folder=" << serverPath << name << "/\" target=\"_self\">" << name << "</button></div>";
		}
		files.sort(compare_nocase);
		for (auto &name : files)
		{
			contentstream << "<div><a class=\"button button1\" href=\"" << serverPath << name << "\" target=\"_top\">" << name << "</a><a class=\"button button2\" href=\"" << serverPath << name << "?herunterladen\"></a></div>";
		}
		contentstream << htmlbottom;
		std::string content(contentstream.str());
		auto & responseHeader = buffer.Response();
		responseHeader.status = Ok;
		ByteArray buf(10);
		snprintf(buf.Data(), buf.Length(), "%zu", content.length());
		responseHeader["Content-Type"] = "text/html; charset=utf-8";
		responseHeader["Content-Length"] = buf.Data();
		responseHeader["Cache-Control"] = "no-store, must-revalidate";
		buffer.Send(responseHeader.toString());
		if (requestHeader.httpMethode != "HEAD")
		{
			buffer.Send(content);
		}
	}
	else if (requestHeader.httpMethode == "POST")
	{
		std::string & contenttype = requestHeader["Content-Type"];
		if (memcmp(contenttype.data(), "multipart/form-data", 19) == 0)
		{
			fs::path serverPath(buffer.RootPath() / requestHeader.putPath);
			if (!fs::is_directory(serverPath))
			{
				throw NotFoundException("Ziel Ordner nicht gefunden");
			}
			struct
			{
				std::string contentSeperator;
				std::ofstream fileStream;
				int proccessedContent;
				int ContentLength;
				fs::path serverPath;
			} args;
			args.contentSeperator = "--" + ParameterValues(contenttype)["boundary"];
			args.proccessedContent = 0;
			args.ContentLength = std::stoull(requestHeader["Content-Length"]);
			args.serverPath = std::move(serverPath);
			buffer.RecvData([&args](RequestBuffer &buffer) -> int
			{
				int contentSeperatormatch, contentSeperatorI = buffer.IndexOf(args.contentSeperator, 0, contentSeperatormatch);//Working ???
				if (args.fileStream.is_open() && (contentSeperatorI == -1 || contentSeperatorI > 2))
				{
					const int data = std::min<unsigned int>(buffer.Length(), contentSeperatorI - 2);
					buffer.CopyTo(args.fileStream, data);
					args.proccessedContent += std::max(data, contentSeperatorI);
					return contentSeperatorI == -1 ? 0 : 2;
				}
				else if (contentSeperatorI != -1 && contentSeperatormatch == args.contentSeperator.length())
				{
					int contentSeperatorEndI = contentSeperatorI + args.contentSeperator.length() + 2;
					int match, fileHeaderEndI = buffer.IndexOf("\r\n\r\n", contentSeperatorEndI, match);
					if (args.fileStream.is_open()) args.fileStream.close();
					if (fileHeaderEndI != -1)
					{
						int proccessed = fileHeaderEndI + match;
						if (match == 4)
						{
							ByteArray range(std::move(buffer.GetRange(contentSeperatorEndI, fileHeaderEndI - contentSeperatorEndI)));
							std::string name = ParameterValues(Parameter(std::string(range.Data(), range.Length()))["Content-Disposition"])["name"];
							args.fileStream.open((args.serverPath / name.substr(1, name.length() - 2)), std::ios::binary);
							args.proccessedContent += proccessed;
						}
						else
						{
							auto &responseHeader = buffer.Response();
							responseHeader.status = Ok;
							buffer.Send(responseHeader.toString());
							return ~proccessed;
						}
						return proccessed;
					}
				}
				return 0;
			});
			return 0;
		}
		else if (memcmp(requestHeader["Content-Type"].data(), "application/x-www-form-urlencoded", 33) == 0)
		{
			ByteArray range(buffer.GetRange(0, std::stoull(requestHeader["Content-Length"])));
			int m, m2, un = range.IndexOf("UserName=", 0, m);
			std::string username(range.Data(), un + m, range.IndexOf("&", un + m, m2) - (un + m));
			int up = range.IndexOf("Password=", un + m, m);
			std::string password(range.Data(), up + m, range.Length() - (up + m));
			ByteArray buf(33);
			for (int i = 0; i < 32; i++)
			{
				buf[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[rand() % 62];
			}
			buf[32] = 0;
			std::string sessionid(buf.Data());
			auto &responseHeader = buffer.Response();
			responseHeader["Set-Cookie"] = "Sessionid=" + sessionid + "; Secure; HttpOnly";
			responseHeader.status = SeeOther;
			responseHeader["Location"] = "/benutzer.cpp";
			buffer.Send(responseHeader.toString());
			benutzer[sessionid] = username;
			return range.Length();
		}
	}
	return 0;
}