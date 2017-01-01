#include "Http2.h"
#define isnan(x) (unsigned char)std::isnan(x)
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
#undef isnan

#include <sstream>
#include <string>
#include <experimental/filesystem>
#include <list>
#include <vector>

#ifndef _WIN32
#define GetThreadId(e) (e)
#endif

using namespace Http2;
namespace fs = std::experimental::filesystem;

#ifndef ZTS
std::mutex phpsync;
#endif

struct PHPClientData
{
	Connection& con;
	Stream& stream;
	std::vector<std::pair<std::string, std::string>> headerlist;
};

static int php_pri_startup(sapi_module_struct *sapi_module)
{
	return php_module_startup(sapi_module, NULL, 0);
}

static size_t sapi_pri_ub_write(const char *str, size_t str_length)
{
	if (!str_length) {
		return 0;
	}
	auto &client = *(PHPClientData*)SG(server_context);
	size_t transfered = 0;
	while (transfered < str_length)
	{
		size_t len = std::min(str_length - transfered, (size_t)16384);
		client.con.winput = std::reverse_copy((unsigned char*)&len, (unsigned char*)&len + 3, client.con.winput);
		*client.con.winput++ = (unsigned char)Frame::Type::DATA;
		*client.con.winput++ = 0;
		client.con.winput = std::reverse_copy((unsigned char*)&client.stream.indentifier, (unsigned char*)&client.stream.indentifier + 4, client.con.winput);
		do
		{
			int res = SSL_write(client.con.cssl, client.con.woutput.Pointer(), client.con.woutput.PointerReadableLength(client.con.winput));
			if (res <= 0)
			{
				throw std::runtime_error("Verbindungsfehler");
			}
			client.con.woutput += res;
		} while (client.con.woutput != client.con.winput);
		if (SSL_write(client.con.cssl, str + transfered, len) <= 0)
		{
			throw std::runtime_error("Verbindungsfehler");
		}
		transfered += len;
	}
	return str_length;
}

static void sapi_pri_flush(void *server_context)
{

}

int header_handler(sapi_header_struct *sapi_header, sapi_header_op_enum op, sapi_headers_struct *sapi_headers)
{
	switch (op)
	{
	case SAPI_HEADER_ADD:

		break;
	case SAPI_HEADER_DELETE:
		break;
	case SAPI_HEADER_DELETE_ALL:
		break;
	case SAPI_HEADER_REPLACE:
		break;
	case SAPI_HEADER_SET_STATUS:

		break;
	default:
		break;
	}
	return 0;
}

static int sapi_pri_send_headers(sapi_headers_struct *sapi_headers)
{
	auto &client = *(PHPClientData*)SG(server_context);
	client.headerlist.insert(client.headerlist.begin(), { ":status", sapi_headers->http_response_code == 0 ? "200" : std::to_string(sapi_headers->http_response_code) });
	client.headerlist.push_back({ "content-type", sapi_headers->mimetype });
	client.con.winput += 3;
	*client.con.winput++ = (unsigned char)Frame::Type::HEADERS;
	*client.con.winput++ = (unsigned char)Frame::Flags::END_HEADERS;
	client.con.winput = std::reverse_copy((unsigned char*)&client.stream.indentifier, (unsigned char*)&client.stream.indentifier + 4, client.con.winput);
	client.con.winput = client.con.hencoder.Headerblock(client.con.winput, client.headerlist);
	{
		uint32_t length = (client.con.winput - client.con.woutput) - 9;
		std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, client.con.woutput);
	}
	do
	{
		int res = SSL_write(client.con.cssl, client.con.woutput.Pointer(), client.con.woutput.PointerReadableLength(client.con.winput));
		if (res <= 0)
		{
			throw std::runtime_error("Verbindungsfehler");
		}
		client.con.woutput += res;
	} while (client.con.woutput != client.con.winput);
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}

static size_t sapi_pri_read_post(char *buf, size_t count_bytes)
{
	return 0;
}

static char* sapi_pri_read_cookies(void)
{
	auto &client = *(PHPClientData*)SG(server_context);
	std::string key = "cookie";
	auto res = std::search(client.stream.headerlist.begin(), client.stream.headerlist.end(), &key, &key + 1, HPack::KeySearch<std::string, std::string>);
	if (res != client.stream.headerlist.end())
	{
		return (char*)res->second.data();
	}
	return NULL;
}


static void sapi_pri_register_variables(zval *track_vars_array)
{
	size_t len;
	char   *docroot = "/";
	php_import_environment_variables(track_vars_array);
	len = 0U;
	if (sapi_module.input_filter(PARSE_SERVER, "DOCUMENT_ROOT", &docroot, len, &len)) {
		php_register_variable("DOCUMENT_ROOT", docroot, track_vars_array);
	}
}

static void sapi_pri_log_message(char *message
#ifdef _WIN32
	, int syslog_type_int
#endif
)
{
	std::cout << "PHP Message: '" << message << "'\n";
}

static sapi_module_struct pri_sapi_module = {
	"pri",							/* name */
	"Program Interface",			/* pretty name */

	php_pri_startup,				/* startup */
	php_module_shutdown_wrapper,	/* shutdown */

	NULL,							/* activate */
	NULL,							/* deactivate */

	sapi_pri_ub_write,		    	/* unbuffered write */
	sapi_pri_flush,				    /* flush */
	NULL,							/* get uid */
	NULL,							/* getenv */

	php_error,						/* error handler */

	header_handler,		/* header handler */
	sapi_pri_send_headers,			/* send headers handler */
	NULL,			/* send header handler */

	sapi_pri_read_post,				 /* read POST data */
	sapi_pri_read_cookies,          /* read Cookies */

	sapi_pri_register_variables,	/* register server variables */
	sapi_pri_log_message,			/* Log message */
	NULL,							/* Get request time */
	NULL,							/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

static int pri_seek_file_begin(zend_file_handle *file_handle, const char *script_file, int *lineno)
{
	int c;

	*lineno = 1;

	file_handle->type = ZEND_HANDLE_FP;
	file_handle->opened_path = NULL;
	file_handle->free_filename = 0;
	if (!(file_handle->handle.fp = fopen(script_file, "rb"))) {
		php_printf("Could not open input file: %s\n", script_file);
		return FAILURE;
	}
	file_handle->filename = script_file;

	/* #!php support */
	c = fgetc(file_handle->handle.fp);
	if (c == '#' && (c = fgetc(file_handle->handle.fp)) == '!') {
		while (c != '\n' && c != '\r' && c != EOF) {
			c = fgetc(file_handle->handle.fp);	/* skip to end of line */
		}
		/* handle situations where line is terminated by \r\n */
		if (c == '\r') {
			if (fgetc(file_handle->handle.fp) != '\n') {
				zend_long pos = zend_ftell(file_handle->handle.fp);
				zend_fseek(file_handle->handle.fp, pos - 1, SEEK_SET);
			}
		}
		*lineno = 2;
	}
	else {
		rewind(file_handle->handle.fp);
	}
	return SUCCESS;
}


void init()
{
	std::cout << "init..";
#ifdef ZTS
	tsrm_startup(4, 4, 0, NULL);
#else
	std::cout << "start sapi" << "\n";
	sapi_startup(&pri_sapi_module);
	std::cout << "start modul" << "\n";
	if (pri_sapi_module.startup(&pri_sapi_module) == FAILURE) {
		std::cout << "module nicht gestartet" << "\n";
		return;
	}
#endif
	std::cout << "Ok\n";
}

void deinit()
{
	std::cout << "deinit..";
	//php_request_shutdown((void *)0);
	pri_sapi_module.shutdown(&pri_sapi_module);
#ifdef ZTS
	tsrm_shutdown();
#endif
	std::cout << "Ok\n";
}

int resources = 0;

void requesthandler(Connection& con, Stream& stream, const fs::path& filepath, const std::string& uri, const std::string& args)
{
	std::cout << "requesthandler..\n";
	try {
		fs::path phpfile = fs::canonical(uri);
		std::string phpfilestring = phpfile.u8string();
		if (fs::is_regular_file(phpfile) && phpfile.extension() == ".php")
		{
			int lineno;
			zend_file_handle file_handle;
			if (pri_seek_file_begin(&file_handle, phpfilestring.c_str(), &lineno) == SUCCESS)
			{
				std::cout << phpfile << "\n";
#ifdef ZTS
				if (tsrm_get_ls_cache() == nullptr)
				{
                    std::cout << "create resource" << "\n";
					ts_resource(resources++);
					if (resources == 1)
					{
					std::cout << "start sapi" << "\n";
						sapi_startup(&pri_sapi_module);
					}
					std::cout << "start modul" << "\n";
					if (pri_sapi_module.startup(&pri_sapi_module) == FAILURE) {
					std::cout << "module nicht gestartet" << "\n";
						return;
					}
				}
#else
				phpsync.lock();
#endif
				{
				std::cout << "info erstellen" << "\n";
					PHPClientData info = { con , stream};
					std::cout << "info erstellt" << "\n";
					SG(headers_sent) = 0;
					SG(server_context) = &info;
					php_request_startup();
					std::cout << "php run" << "\n";
					php_execute_script(&file_handle);
					php_request_shutdown(nullptr);
					SG(server_context) = 0;
				}
#ifndef ZTS
phpsync.unlock();
#endif
				{
					long long count = 0;
					con.winput = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, con.winput);
				}
				*con.winput++ = (unsigned char)Frame::Type::DATA;
				*con.winput++ = (unsigned char)Frame::Flags::END_STREAM;
				con.winput = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, con.winput);
				do
				{
					con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
				} while (con.woutput != con.winput);
			}
			else
			{
				throw std::runtime_error("Kann nicht geöffnet werden");
			}
		}
		else
		{
			throw std::runtime_error("Kein PHP-Skript");
		}
	}
	catch (std::exception &ex)
	{
		std::vector<std::pair<std::string, std::string>> headerlist;
		headerlist.push_back({ ":status", "404" });
		con.winput += 3;
		*con.winput++ = (unsigned char)Frame::Type::HEADERS;
		*con.winput++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
		con.winput = std::reverse_copy((unsigned char*)&stream.indentifier, (unsigned char*)&stream.indentifier + 4, con.winput);
		con.winput = con.hencoder.Headerblock(con.winput, headerlist);
		{
			uint32_t length = (con.winput - con.woutput) - 9;
			std::reverse_copy((unsigned char*)&length, (unsigned char*)&length + 3, con.woutput);
		}
		do
		{
			con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
		} while (con.woutput != con.winput);
	}
	std::cout << "..requesthandler Ok\n";
}