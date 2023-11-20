#ifndef HTTP_REST_H
#define HTTP_REST_H

#include "esp_http_server.h"

esp_err_t http_rest_get_handler(httpd_req_t* req);

esp_err_t http_rest_post_handler(httpd_req_t* req);

esp_err_t http_rest_restart_post_handler(httpd_req_t* req);

esp_err_t http_rest_state_post_handler(httpd_req_t* req);

esp_err_t http_rest_firmware_update_post_handler(httpd_req_t* req);

esp_err_t http_rest_firmware_upload_post_handler(httpd_req_t* req);

esp_err_t http_rest_script_reload_post_handler(httpd_req_t* req);

esp_err_t http_rest_partition_get_handler(httpd_req_t* req);

esp_err_t http_rest_fs_file_get_handler(httpd_req_t* req);

esp_err_t http_rest_fs_file_post_handler(httpd_req_t* req);

esp_err_t http_rest_fs_file_delete_handler(httpd_req_t* req);

esp_err_t http_rest_log_get_handler(httpd_req_t* req);

esp_err_t http_rest_script_output_get_handler(httpd_req_t* req);

#endif /* HTTP_REST_H */