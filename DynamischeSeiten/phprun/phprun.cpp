#include "Http2.h"

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

#include <sstream>
#include <string>
#include <experimental/filesystem>
#include <list>

#ifndef _WIN32
#define GetThreadId(e) (e)
#endif

using namespace Http2;
namespace fs = std::experimental::filesystem;

//std::mutex phpmtx;

struct PHPClientData
{
	Connection& con;
	Stream& stream;
	Utility::Array<std::pair<std::string, std::string>> headerlist;
	Utility::RotateIterator<std::pair<std::string, std::string>> headerlistend;
};

static int php_pri_startup(sapi_module_struct *sapi_module)
{
	if (php_module_startup(sapi_module, NULL, 0) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static size_t sapi_pri_ub_write(const char *str, size_t str_length)
{
	auto glob = TSRMG_BULK(sapi_globals_id, sapi_globals_struct*);
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
		client.con.winput = std::reverse_copy((unsigned char*)&client.stream.streamIndentifier, (unsigned char*)&client.stream.streamIndentifier + 4, client.con.winput);
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

	//memset((*((void ***)ZEND_TSRMLS_CACHE))[TSRM_UNSHUFFLE_RSRC_ID(output_globals_id)], 0, sizeof(zend_output_globals));
	//TSRM_UNSHUFFLE_RSRC_ID(s);
	//ZEND_TSRMLS_CACHE;
	//std::thread::native_handle();
	auto glob = TSRMG_BULK(sapi_globals_id, sapi_globals_struct*);
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
	auto glob = TSRMG_BULK(sapi_globals_id, sapi_globals_struct*);
	auto &client = *(PHPClientData*)SG(server_context);
	*client.headerlist.begin() = { ":status", sapi_headers->http_response_code == 0 ? "200" : std::to_string(sapi_headers->http_response_code) };
	*client.headerlistend++ = { "content-type", sapi_headers->mimetype };
	client.con.winput += 3;
	*client.con.winput++ = (unsigned char)Frame::Type::HEADERS;
	*client.con.winput++ = (unsigned char)Frame::Flags::END_HEADERS;
	client.con.winput = std::reverse_copy((unsigned char*)&client.stream.streamIndentifier, (unsigned char*)&client.stream.streamIndentifier + sizeof(client.stream.streamIndentifier), client.con.winput);
	client.con.winput = client.con.hencoder.Headerblock(client.con.winput, client.headerlist.begin(), client.headerlistend);
	{
		uint32_t l = (client.con.winput - client.con.woutput) - 9;
		std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, client.con.woutput);
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

void send_header(sapi_header_struct *sapi_header, void *server_context)
{
	__debugbreak();
}

static size_t sapi_pri_read_post(char *buf, size_t count_bytes)
{
	return 0;
}

static char* sapi_pri_read_cookies(void)
{

	return NULL;
}


static void sapi_pri_register_variables(zval *track_vars_array)
{
	size_t len;
	char   *docroot = ".";
	php_import_environment_variables(track_vars_array);
	len = 0U;
	if (sapi_module.input_filter(PARSE_SERVER, "DOCUMENT_ROOT", &docroot, len, &len)) {
		php_register_variable("DOCUMENT_ROOT", docroot, track_vars_array);
	}
}

static void sapi_pri_log_message(char *message, int syslog_type_int)
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
	send_header,			/* send header handler */

	sapi_pri_read_post,				 /* read POST data */
	sapi_pri_read_cookies,          /* read Cookies */

	sapi_pri_register_variables,	/* register server variables */
	sapi_pri_log_message,			/* Log message */
	NULL,							/* Get request time */
	NULL,							/* Child terminate */

	STANDARD_SAPI_MODULE_PROPERTIES
};

//const char HARDCODED_INI[] =
//"html_errors=0\n"
//"register_argc_argv=0\n"
//"implicit_flush=0\n"
//"output_buffering=0\n"
//"max_execution_time=0\n"
//"max_input_time=-1\n\0";

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
#ifdef ZTS
	tsrm_startup(4, 4, 0, NULL);
#endif
	pri_sapi_module.php_ini_path_override = nullptr;
	pri_sapi_module.phpinfo_as_text = 0;
	pri_sapi_module.php_ini_ignore_cwd = 0;

	pri_sapi_module.php_ini_ignore = 0;
}

void deinit()
{
	php_request_shutdown((void *)0);
	pri_sapi_module.shutdown(&pri_sapi_module);
#ifdef ZTS
	tsrm_shutdown();
#endif
}

int resources = 0;

void requesthandler(Connection& con, Stream& stream, const fs::path& filepath, const std::string& uri, const std::string& args)
{
	try {
		fs::path phpfile = fs::canonical(uri);
		std::string phpfilestring = phpfile.u8string();
		if (fs::is_regular_file(phpfile) && phpfile.extension() == ".php")
		{
			int lineno;
			zend_file_handle file_handle;
			if (pri_seek_file_begin(&file_handle, phpfilestring.c_str(), &lineno) == SUCCESS)
			{
				if (tsrm_get_ls_cache() == nullptr)
				{
					ts_resource(resources++);
					auto cache = tsrm_get_ls_cache();
					if (resources == 1)
					{
						sapi_startup(&pri_sapi_module);
					}
					if (pri_sapi_module.startup(&pri_sapi_module) == FAILURE) {
						return;
					}
					php_request_startup();
				}
				{
					PHPClientData info = { con , stream, 3 };
					info.headerlistend = info.headerlist.begin() + 1;
					SG(headers_sent) = 0;
					SG(server_context) = &info;
					zend_unset_timeout();
					//TSRMG_BULK(executor_globals_id, _zend_executor_globals*)->ticks_count = 0;
					//(void)ts_resource(0);
					//auto before = TSRMG_BULK(executor_globals_id, zend_executor_globals*);
					//if (before == nullptr)
					//{
					//	__debugbreak();
					//}
					auto glob = TSRMG_BULK(sapi_globals_id, sapi_globals_struct*);
					php_execute_script(&file_handle);
					//auto after = TSRMG_BULK(executor_globals_id, zend_executor_globals*);
					//if (after == nullptr)
					//{
					//	__debugbreak();
					//}
					EG(timed_out);
					SG(server_context) = 0;
				}
				//phpmtx.unlock();
				{
					long long count = 0;
					con.winput = std::reverse_copy((unsigned char*)&count, (unsigned char*)&count + 3, con.winput);
				}
				*con.winput++ = (unsigned char)Frame::Type::DATA;
				*con.winput++ = (unsigned char)Frame::Flags::END_STREAM;
				con.winput = std::reverse_copy((unsigned char*)&stream.streamIndentifier, (unsigned char*)&stream.streamIndentifier + 4, con.winput);
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
		Utility::Array<std::pair<std::string, std::string>> headerlist(1);
		*headerlist.data() = { ":status", "404" };
		con.winput += 3;
		*con.winput++ = (unsigned char)Frame::Type::HEADERS;
		*con.winput++ = (unsigned char)Frame::Flags::END_HEADERS | (unsigned char)Frame::Flags::END_STREAM;
		con.winput = std::reverse_copy((unsigned char*)&stream.streamIndentifier, (unsigned char*)&stream.streamIndentifier + sizeof(stream.streamIndentifier), con.winput);
		con.winput = con.hencoder.Headerblock(con.winput, headerlist.begin(), headerlist.end());
		{
			uint32_t l = (con.winput - con.woutput) - 9;
			std::reverse_copy((unsigned char*)&l, (unsigned char*)&l + 3, con.woutput);
		}
		do
		{
			con.woutput += SSL_write(con.cssl, con.woutput.Pointer(), con.woutput.PointerReadableLength(con.winput));
		} while (con.woutput != con.winput);
	}
}