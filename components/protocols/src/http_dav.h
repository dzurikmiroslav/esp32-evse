#ifndef HTTP_DAV_H
#define HTTP_DAV_H

#include <esp_http_server.h>

size_t http_dav_handlers_count(void);

void http_dav_add_handlers(httpd_handle_t server);

#endif /* HTTP_DAV_H */