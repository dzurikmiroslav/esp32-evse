#ifndef HTTP_WEB_H
#define HTTP_WEB_H

#include <esp_http_server.h>

size_t http_web_handlers_count(void);

void http_web_add_handlers(httpd_handle_t server);

#endif /* HTTP_WEB_H */