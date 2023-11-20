#ifndef HTTP_DAV_H
#define HTTP_DAV_H

#include "esp_http_server.h"

esp_err_t http_dav_propfind_handler(httpd_req_t* req);

esp_err_t http_dav_get_handler(httpd_req_t* req);

esp_err_t http_dav_options_handler(httpd_req_t* req);

#endif /* HTTP_DAV_H */