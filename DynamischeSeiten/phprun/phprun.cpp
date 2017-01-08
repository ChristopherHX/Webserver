#include <php.h>
#include <SAPI.h>
#include <php_globals.h>
#include <php_variables.h>
#include <zend_hash.h>
#include <zend_modules.h>
#include <zend_interfaces.h>
#include <zend.h>
#include <zend_extensions.h>
#include <php_main.h>
#undef inline

#include "Http2.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <experimental/filesystem>
#include <list>
#include <vector>

using namespace Http2;
namespace fs = std::experimental::filesystem;

#ifndef ZTS
std::mutex phpsync;
#endif

struct PHPClientData
{
	Connection& con;
	Stream& stream;
	std::vector<std::pair<std::string, std::string>> env;
};

static int php_phprun_startup(sapi_module_struct *sapi_module)
{
	return php_module_startup(sapi_module, NULL, 0);
}

static size_t sapi_phprun_ub_write(const char *str, size_t str_length)
{
	if (!str_length) {
		return 0;
	}
	auto &client = *(PHPClientData*)SG(server_context);
	size_t transfered = 0;
	std::vector<uint8_t> buffer(9);
	auto wpos = buffer.begin();
	wpos += 3;
	*wpos++ = (unsigned char)Frame::Type::DATA;
	*wpos++ = 0;
	wpos = std::reverse_copy((unsigned char*)&client.stream.indentifier, (unsigned char*)&client.stream.indentifier + 4, wpos);
	while (transfered < str_length)
	{
		const uint32_t &len = std::min((uint32_t)(str_length - transfered), client.con.settings[(uint16_t)Settings::MAX_FRAME_SIZE]);
		std::reverse_copy((unsigned char*)&len, (unsigned char*)&len + 3, buffer.begin());
		if (SSL_write(client.con.cssl, buffer.data(), buffer.size()) <= 0)
		{
			throw std::runtime_error("Verbindungsfehler");
		}
		if (SSL_write(client.con.cssl, str + transfered, len) <= 0)
		{
			throw std::runtime_error("Verbindungsfehler");
		}
		transfered += len;
	}
	return str_length;
}

static void sapi_phprun_flush(void *server_context)
{

}

static int sapi_phprun_send_headers(sapi_headers_struct *sapi_headers)
{
	auto &client = *(PHPClientData*)SG(server_context);
	std::vector<std::pair<std::string, std::string>> headerlist;
	headerlist.push_back({ ":status", sapi_headers->http_response_code == 0 ? "200" : std::to_string(sapi_headers->http_response_code) });
	{
		sapi_header_struct * sapi_header = (sapi_header_struct *)zend_llist_get_first(&sapi_headers->headers);
		for (size_t i = 0; i < sapi_headers->headers.count; i++)
		{
			auto res = std::find(sapi_header->header, sapi_header->header + sapi_header->header_len, ':');
			if (res != (sapi_header->header + sapi_header->header_len))
			{
				int l = *(res + 1) == ' ' ? 2 : 1;
				std::string key(sapi_header->header, res);
				std::transform(key.begin(), key.end(), key.begin(), tolower);
				headerlist.push_back({ key, std::string(res + l, sapi_header->header + sapi_header->header_len) });
				sapi_header = (sapi_header_struct *)zend_llist_get_next(&sapi_headers->headers);
			}
		}
	}
	std::vector<uint8_t> buffer(9 + client.con.settings[(uint32_t)Settings::MAX_HEADER_LIST_SIZE]);
	auto wpos = buffer.begin();
	wpos += 3;
	*wpos++ = (unsigned char)Frame::Type::HEADERS;
	*wpos++ = (unsigned char)Frame::Flags::END_HEADERS;
	wpos = std::reverse_copy((unsigned char*)&client.stream.indentifier, (unsigned char*)&client.stream.indentifier + 4, wpos);
	wpos = client.con.hencoder.Headerblock(wpos, headerlist);
	{
		uint32_t length = (wpos - buffer.begin()) - 9;
		std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, buffer.begin());
	}
	client.con.wmtx.lock();
	if (SSL_write(client.con.cssl, buffer.data(), buffer.size()) <= 0)
	{
		throw std::runtime_error("Verbindungsfehler");
	}
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static size_t sapi_phprun_read_post(char *buf, size_t count_bytes)
{
	auto &client = *(PHPClientData*)SG(server_context);
	std::vector<uint8_t> buffer(9);
	if (ReadUntil(client.con.cssl, buffer.data(), 9))
	{
		Frame frame = ReadFrame(buffer.begin());
		if (frame.type == Frame::Type::DATA && frame.length <= client.con.settings[(uint16_t)Settings::MAX_FRAME_SIZE])
		{
			if (count_bytes < frame.length) throw std::runtime_error("PHP PostBuffer zu klein");
			return SSL_read(client.con.cssl, buf, frame.length);
		}
	}
	throw std::runtime_error("PHP Post Read Fehlgeschlagen");
	return -1;
}

static char* sapi_phprun_read_cookies(void)
{
	auto &client = *(PHPClientData*)SG(server_context);
	auto res = std::find_if(client.stream.headerlist.begin(), client.stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "cookie"; });
	if (res != client.stream.headerlist.end())
	{
		return (char*)res->second.data();
	}
	return NULL;
}


static void sapi_phprun_register_variables(zval *track_vars_array)
{
	auto &client = *(PHPClientData*)SG(server_context);
	for (auto &variable : client.env)
	{
		size_t len;
		const char* value = variable.second.data();
		if (sapi_module.input_filter(PARSE_SERVER, (char*)variable.first.data(), (char**)&value, variable.second.length(), &len)) {
			php_register_variable((char*)variable.first.data(), (char*)variable.second.data(), track_vars_array);
		}
	}
}

static void sapi_phprun_log_message(char *message
#ifdef _WIN32
	, int syslog_type_int
#endif
)
{
	std::cout << "PHP Message: '" << message << "'\n";
}

static sapi_module_struct phprun_sapi_module = {
	"phprun",						/* name */
	"PHP Runner",					/* pretty name */

	php_phprun_startup,				/* startup */
	php_module_shutdown_wrapper,	/* shutdown */

	NULL,							/* activate */
	NULL,							/* deactivate */

	sapi_phprun_ub_write,		    /* unbuffered write */
	sapi_phprun_flush,				/* flush */
	NULL,							/* get uid */
	NULL,							/* getenv */

	php_error,						/* error handler */

	NULL/*header_handler*/,					/* header handler */
	sapi_phprun_send_headers,		/* send headers handler */
	NULL,							/* send header handler */

	sapi_phprun_read_post,			/* read POST data */
	sapi_phprun_read_cookies,		/* read Cookies */

	sapi_phprun_register_variables,	/* register server variables */
	sapi_phprun_log_message,		/* Log message */
	/*php_phprun_get_request_time*/NULL,	/* Get request time */
	NULL,							/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

void init()
{
#ifdef ZTS
	tsrm_startup(4, 4, 0, NULL);
#else
	sapi_startup(&phprun_sapi_module);
	if (phprun_sapi_module.startup(&phprun_sapi_module) == FAILURE) {
		std::cout << "module nicht gestartet" << "\n";
		return;
	}
#endif
}

void deinit()
{
	phprun_sapi_module.shutdown(&phprun_sapi_module);
#ifdef ZTS
	tsrm_shutdown();
#endif
}

int resources = 0;

void requesthandler(Server & server, Connection & con, Stream & stream, fs::path & filepath, std::string & uri, std::string & args)
{
	fs::path phpfile = server.GetRootPath() / uri;
	std::string uri2;
	if (Http2::FindFile(phpfile, uri2) && phpfile.extension() == ".php")
	{
#ifdef ZTS
		if (tsrm_get_ls_cache() == nullptr)
		{
			ts_resource(resources++);
			if (resources == 1)
			{
				sapi_startup(&phprun_sapi_module);
			}
			if (phprun_sapi_module.startup(&phprun_sapi_module) == FAILURE) {
				std::cout << "module nicht gestartet" << "\n";
				return;
			}
		}
#endif
		{
			zend_file_handle file_handle;
			PHPClientData info = { con , stream };
			std::string phpfilestring = phpfile.u8string();
			file_handle.type = ZEND_HANDLE_FILENAME;
			file_handle.filename = (char *)phpfilestring.c_str();
			file_handle.free_filename = 0;
			file_handle.opened_path = NULL;

			SG(request_info).query_string = (char*)args.c_str();
			SG(request_info).proto_num = 200;
			SG(request_info).path_translated = (char *)phpfilestring.c_str();
			info.env.push_back({ "DOCUMENT_ROOT", server.GetRootPath().u8string() });
			info.env.push_back({ "HTTPS", "1" });
			info.env.push_back({ "SERVER_PROTOCOL", "HTTP/2.0" });
			info.env.push_back({ "HTTP_CONNECTION", "Keep-Alive" });
			info.env.push_back({ "SCRIPT_FILENAME", phpfilestring });
			info.env.push_back({ "QUERY_STRING", args });
			{
				auto res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":path"; });
				if (res != stream.headerlist.end())
				{
					SG(request_info).request_uri = (char*)res->second.c_str();
					info.env.push_back({ "REQUEST_URI", res->second });
					{
						std::string val = res->second.substr(0, res->second.find('?'));
						info.env.push_back({ "PHP_SELF", val });
						info.env.push_back({ "SCRIPT_NAME", val });
					}
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":authority"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_HOST", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":method"; });
				if (res != stream.headerlist.end())
				{
					SG(request_info).request_method = res->second.c_str();
					info.env.push_back({ "REQUEST_METHOD", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "content-length"; });
				if (res != stream.headerlist.end())
				{
					SG(request_info).content_length = (int64_t)std::stoll(res->second);
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "content-type"; });
				if (res != stream.headerlist.end())
				{
					SG(request_info).content_type = res->second.c_str();
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "accept"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_ACCEPT", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "accept-charset"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_ACCEPT_CHARSET", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "accept-encoding"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_ACCEPT_ENCODING", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "accept-language"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_ACCEPT_LANGUAGE", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "referer"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_REFERER", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == ":autority"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_HOST", res->second });
				}
				res = std::find_if(stream.headerlist.begin(), stream.headerlist.end(), [](const std::pair<std::string, std::string> & pair) { return pair.first == "user-agent"; });
				if (res != stream.headerlist.end())
				{
					info.env.push_back({ "HTTP_USER_AGENT", res->second });
				}
			}
#ifndef ZTS
			phpsync.lock();
#endif
			SG(server_context) = &info;
			php_request_startup();
			php_execute_script(&file_handle);
			php_request_shutdown(nullptr);
			SG(server_context) = nullptr;
#ifndef ZTS
			phpsync.unlock();
#endif
		}
		{
			std::vector<uint8_t> buffer(9);
			auto wpos = buffer.begin();
			{
				long long count = 0;
				wpos = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, wpos);
			}
			*wpos++ = (unsigned char)Frame::Type::DATA;
			*wpos++ = (unsigned char)Frame::Flags::END_STREAM;
			wpos = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, wpos);
			if (SSL_write(con.cssl, buffer.data(), buffer.size()) <= 0)
			{
				throw std::runtime_error("Verbindungsfehler");
			}
			con.wmtx.unlock();
		}
	}
	else
	{
		return server.filehandler(server, con, stream, phpfile, uri2, args);
	}
}