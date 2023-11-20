#ifndef HTTP_WEB_H
#define HTTP_WEB_H

#include "esp_http_server.h"

esp_err_t http_web_get_handler(httpd_req_t* req);

#endif /* HTTP_WEB_H */