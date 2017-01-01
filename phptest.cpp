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
#include <iostream>
#include <string>

static int php_pri_startup(sapi_module_struct *sapi_module) /* {{{ */
{
	if (php_module_startup(sapi_module, NULL, 0) == FAILURE) {
		return FAILURE;
	}
	return SUCCESS;
}

static size_t sapi_pri_ub_write(const char *str, size_t str_length) /* {{{ */
{
	if (!str_length) {
		return 0;
	}

	std::cout << "PHP echo: '" << std::string(str, str_length) << "'\n";
	return str_length;
}

static void sapi_pri_flush(void *server_context) /* {{{ */
{
	
}

static int sapi_pri_send_headers(sapi_headers_struct *sapi_headers) /* {{{ */
{
	char* test = (char*)SG(server_context);
	/* We do nothing here, this function is needed to prevent that the fallback
	* header handling is called. */
	std::cout << "SAPI_HEADER_Send:'" << sapi_headers->mimetype << "'\n";
	return SAPI_HEADER_SENT_SUCCESSFULLY;
}
/* }}} */

static size_t sapi_pri_read_post(char *buf, size_t count_bytes) /* {{{ */
{
	return 0;
}

static char* sapi_pri_read_cookies(void) /* {{{ */
{

	return NULL;
}


static void sapi_pri_register_variables(zval *track_vars_array) /* {{{ */
{
	size_t len;
	char   *docroot = "";

	/* In CGI mode, we consider the environment to be a part of the server
	* variables
	*/
	php_import_environment_variables(track_vars_array);

	/* Build the special-case PHP_SELF variable for the CLI version */
	//len = strlen(php_self);
	//if (sapi_module.input_filter(PARSE_SERVER, "PHP_SELF", &php_self, len, &len)) {
	//	php_register_variable("PHP_SELF", php_self, track_vars_array);
	//}
	//if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_NAME", &php_self, len, &len)) {
	//	php_register_variable("SCRIPT_NAME", php_self, track_vars_array);
	//}
	/* filenames are empty for stdin */
	char* script_filename = "test.php";
	len = strlen(script_filename);
	if (sapi_module.input_filter(PARSE_SERVER, "SCRIPT_FILENAME", &script_filename, len, &len)) {
		php_register_variable("SCRIPT_FILENAME", script_filename, track_vars_array);
	}
	if (sapi_module.input_filter(PARSE_SERVER, "PATH_TRANSLATED", &script_filename, len, &len)) {
		php_register_variable("PATH_TRANSLATED", script_filename, track_vars_array);
	}
	/* just make it available */
	len = 0U;
	if (sapi_module.input_filter(PARSE_SERVER, "DOCUMENT_ROOT", &docroot, len, &len)) {
		php_register_variable("DOCUMENT_ROOT", docroot, track_vars_array);
	}
}

static void sapi_pri_log_message(char *message) /* {{{ */
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

	NULL,		/* header handler */
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

const char HARDCODED_INI[] =
"html_errors=0\n"
"register_argc_argv=1\n"
"implicit_flush=1\n"
"output_buffering=0\n"
"max_execution_time=0\n"
"max_input_time=-1\n\0";

static int pri_seek_file_begin(zend_file_handle *file_handle, char *script_file, int *lineno)
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

int main(int argc, char ** argv)
{
#ifdef ZTS
	tsrm_startup(1, 1, 0, NULL);
	(void)ts_resource(0);
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	//pri_sapi_module.ini_defaults = sapi_pri_ini_defaults;
	pri_sapi_module.php_ini_path_override = nullptr;
	pri_sapi_module.phpinfo_as_text = 1;
	pri_sapi_module.php_ini_ignore_cwd = 0;
	sapi_startup(&pri_sapi_module);
	pri_sapi_module.php_ini_ignore = 0;
	pri_sapi_module.executable_location = argv[0];

	pri_sapi_module.ini_entries = new char[sizeof(HARDCODED_INI)];
	memcpy(pri_sapi_module.ini_entries, HARDCODED_INI, sizeof(HARDCODED_INI));
	if (pri_sapi_module.startup(&pri_sapi_module) == FAILURE) {
		delete[] pri_sapi_module.ini_entries;
		return 1;
	}
	
	std::cout << "SG(server_context) = (void*)\"Hallo Server\"\n";
	SG(server_context) = (void*)"Hallo Server";

	zend_file_handle file_handle;
	int lineno = 0;
	pri_seek_file_begin(&file_handle, "/mnt/c/Users/Christopher/Desktop/hello.php", &lineno);
	file_handle.type = ZEND_HANDLE_FP;
	file_handle.opened_path = NULL;
	file_handle.free_filename = 0;
std::cout << "PHP soll gestartet werden\n";
	if (php_request_startup() == FAILURE) {

	}
	std::cout << "PHP wird gestartet\n";
	php_execute_script(&file_handle);
	std::cout << "PHP beendet\n";
	pri_seek_file_begin(&file_handle, "/mnt/c/Users/Christopher/Desktop/hello.php", &lineno);
	file_handle.type = ZEND_HANDLE_FP;
	file_handle.opened_path = NULL;
	file_handle.free_filename = 0;
	std::cout << "PHP wird gestartet\n";
	php_execute_script(&file_handle);
	std::cout << "PHP beendet\n";
	php_request_shutdown((void *)0);

	delete[] pri_sapi_module.ini_entries;
	return 0;
}