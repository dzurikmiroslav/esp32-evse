#ifndef HTTP_H
#define HTTP_H

#include <esp_http_server.h>

void http_init(void);

bool http_authorize_req(httpd_req_t *req);

void http_set_credentials(const char *user, const char *password);

#endif /* HTTP_H */