#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "http_web.h"
#include "http.h"


esp_err_t http_dav_propfind_handler(httpd_req_t* req)
{
    httpd_resp_send_chunk(req, "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n", -1);
    httpd_resp_send_chunk(req, "<multistatus xmlns=\"DAV:\">\n", -1);

    httpd_resp_send_chunk(req, "  <response>\n", -1);
    httpd_resp_send_chunk(req, "      <href>/item.txt</href>\n", -1);
    httpd_resp_send_chunk(req, "      <status>HTTP/1.1 200 OK</status>\n", -1);
    httpd_resp_send_chunk(req, "  </response>\n", -1);

    httpd_resp_send_chunk(req, "</multistatus>\n", -1);
    
    httpd_resp_send_chunk(req, NULL, 0);

    httpd_resp_set_status(req, "207 Multi-Status");

    return ESP_OK;
}

esp_err_t http_dav_get_handler(httpd_req_t* req)
{   
//    httpd_send(req, "OK", 0);
    
    httpd_resp_send(req, "OK", -1);

    return ESP_OK;
}

esp_err_t http_dav_options_handler(httpd_req_t* req)
{   
    return ESP_OK;
}