#ifndef HTTP_REST_H
#define HTTP_REST_H

#include <esp_http_server.h>

size_t http_rest_handlers_count(void);

void http_rest_add_handlers(httpd_handle_t server);

#endif /* HTTP_REST_H */