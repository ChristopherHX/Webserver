////#include <php.h>
////#include <SAPI.h>
//#include <php_ini.h>
//#include <php_variables.h>
//#include <php_main.h>
////#include "ext/standard/php_standard.h"
////#include "zend.h"
////#include "zend_types.h"
////#include "zend_extensions.h"
//
//#include "HttpHeader.h"
//#include "RequestBuffer.h"
//#include "HttpServer.h"
//#include "ByteArray.h"
//#include <sstream>
//#include <experimental/filesystem>
//#include <list>
//
//namespace fs = std::experimental::filesystem;
//
//static zend_module_entry php_apache_module;
//
//static int php_startup(sapi_module_struct *sapi_module)
//{
//	if (php_module_startup(sapi_module, &php_apache_module, 1) == 1) {
//		return 1;
//	}
//	return 0;
//}
//
//static sapi_module_struct phpplugin = {
//	"phpplugin",
//	"PHP Plugin alpha",
//	php_startup
//};
//
//int seite(fs::path & rootfolder, Http::RequestHeader & requestHeader, Http::ResponseHeader& responseHeader, Http::RequestBuffer& buffer, DualByteArray& array)
//{
//	sapi_startup(&phpplugin);
//	
//	//ByteArray buf(64);
//	//std::stringstream contentstream;
//	//contentstream << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"/></head><body>";
//	//sscanf(requestHeader["Cookie"].data(), "Sessionid=%s", buf.Data());
//	//contentstream << "<h1>Willkommen " << (buf.Data() != "" ? buf.Data() : "Melde dich an!!!") << " </h1>";
//	//contentstream << "</body>";
//	//std::string content(contentstream.str());
//	//responseHeader.status = Ok;
//	//snprintf(buf.Data(), buf.Length(), "%zu", content.length());
//	//responseHeader["Content-Type"] = "text/html; charset=utf-8";
//	//responseHeader["Content-Length"] = buf.Data();
//	//responseHeader["Cache-Control"] = "no-store, must-revalidate";
//	//buffer.Send(responseHeader.toString());
//	//if (requestHeader.httpMethode != "HEAD")
//	//{
//	//	buffer.Send(content);
//	//}
//	return 0;
//}