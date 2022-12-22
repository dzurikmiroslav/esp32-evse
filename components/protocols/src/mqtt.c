#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "nvs.h"

#include "mqtt.h"
#include "json.h"
#include "board_config.h"
#include "timeout_utils.h"

#define NVS_NAMESPACE       "mqtt"
#define NVS_ENABLED         "enabled"
#define NVS_SERVER          "server"
#define NVS_BASE_TOPIC      "base_topic"
#define NVS_USER            "user"
#define NVS_PASSWORD        "password"
#define NVS_PERIODICITY     "periodicity"

static const char* TAG = "mqtt";

static nvs_handle nvs;

static TaskHandle_t client_task = NULL;

static esp_mqtt_client_handle_t client = NULL;

static uint16_t periodicity = 30;

static void subcribe_topics(void)
{
    char topic[48];

    mqtt_get_base_topic(topic);
    strcat(topic, "/request/#");
    esp_mqtt_client_subscribe(client, topic, 0);

    mqtt_get_base_topic(topic);
    strcat(topic, "/set/config/#");
    esp_mqtt_client_subscribe(client, topic, 0);

    mqtt_get_base_topic(topic);
    strcat(topic, "/enable");
    esp_mqtt_client_subscribe(client, topic, 0);
}

static void publish_message(const char* topic, cJSON* root)
{
    char target_topic[48];

    mqtt_get_base_topic(target_topic);
    strcat(target_topic, topic);

    const char* json = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(client, target_topic, json, 0, 1, 0);
    free((void*)json);
}

static void handle_message(const char* topic, const char* data)
{
    char base_topic[32];
    mqtt_get_base_topic(base_topic);

    if (strncmp(topic, base_topic, strlen(base_topic)) == 0) {
        const char* sub_topic = &topic[strlen(base_topic)];

        if (strcmp(sub_topic, "/request/config/evse") == 0) {
            cJSON* root = json_get_evse_config();
            publish_message("/response/config/evse", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/config/wifi") == 0) {
            cJSON* root = json_get_wifi_config();
            publish_message("/response/config/wifi", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/config/mqtt") == 0) {
            cJSON* root = json_get_mqtt_config();
            publish_message("/response/config/mqtt", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/config/tcpLogger") == 0) {
            cJSON* root = json_get_tcp_logger_config();
            publish_message("/response/config/tcpLogger", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/config/rest") == 0) {
            cJSON* root = json_get_rest_config();
            publish_message("/response/config/rest", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/boardConfig") == 0) {
            cJSON* root = json_get_board_config();
            publish_message("/response/boardConfig", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/info") == 0) {
            cJSON* root = json_get_info();
            publish_message("/response/info", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/restart") == 0) {
            timeout_restart();
        } else if (strcmp(sub_topic, "/set/config/evse") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_evse_config(root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/wifi") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_wifi_config(root, true);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/mqtt") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_mqtt_config(root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/tcpLogger") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_tcp_logger_config(root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/rest") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_rest_config(root, false);
            cJSON_Delete(root);
        }
    }
}

static void event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    char topic[48];
    char data[256];

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        vTaskResume(client_task);
        subcribe_topics();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        break;
    case MQTT_EVENT_DATA:
        memset(topic, 0, sizeof(topic));
        strncpy(topic, event->topic, MIN(event->topic_len, sizeof(topic) - 1));
        memset(data, 0, sizeof(data));
        strncpy(data, event->data, MIN(event->data_len, sizeof(data) - 1));
        handle_message(topic, data);
        break;
    default:
        break;
    }
}

static void client_task_func(void* param)
{
    while (true) {
        if (!client) {
            vTaskSuspend(NULL);
        }

        cJSON* root = json_get_state();
        publish_message("/state", root);
        cJSON_Delete(root);

        vTaskDelay(pdMS_TO_TICKS(periodicity * 1000));
    }
}

static void client_start(void)
{
    char server[64];
    char user[32];
    char password[64];

    mqtt_get_server(server);
    mqtt_get_user(user);
    mqtt_get_password(password);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = server,
        .credentials.username = user,
        .credentials.authentication.password = password
    };

    if (client) {
        if (esp_mqtt_set_config(client, &cfg) != ESP_OK) {
            ESP_LOGW(TAG, "Cant set config");
        }
    } else {
        client = esp_mqtt_client_init(&cfg);
        if (esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, event_handler, client) != ESP_OK) {
            ESP_LOGW(TAG, "Cant register handler");
        }
        if (client == NULL) {
            ESP_LOGW(TAG, "Cant set config");
        } else {
            if (esp_mqtt_client_start(client) != ESP_OK) {
                ESP_LOGW(TAG, "Cant start");
            }
        }
    }
}

static void client_stop(void)
{
    if (client != NULL) {
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
}

void mqtt_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    nvs_get_u16(nvs, NVS_PERIODICITY, &periodicity);

    esp_register_shutdown_handler(&client_stop);

    xTaskCreate(client_task_func, "mqtt_client_task", 4096, NULL, 5, &client_task);

    if (mqtt_get_enabled()) {
        client_start();
    }
}

esp_err_t mqtt_set_config(bool enabled, const char* server, const char* base_topic, const char* user, const char* password, uint16_t _periodicity)
{
    if (enabled) {
        if (server == NULL || strlen(server) == 0) {
            size_t len = 0;
            nvs_get_str(nvs, NVS_SERVER, NULL, &len);
            if (len <= 1) {
                ESP_LOGE(TAG, "Required server");
                return ESP_ERR_INVALID_ARG;
            }
        }

        if (base_topic == NULL || strlen(base_topic) == 0) {
            size_t len = 0;
            nvs_get_str(nvs, NVS_BASE_TOPIC, NULL, &len);
            if (len <= 1) {
                ESP_LOGE(TAG, "Required base topic");
                return ESP_ERR_INVALID_ARG;
            }
        }

        if (_periodicity == 0) {
            ESP_LOGE(TAG, "Periodicity muse be larger than zero");
            return ESP_ERR_INVALID_ARG;
        }
    }

    if (server != NULL && strlen(server) > 63) {
        ESP_LOGE(TAG, "Server out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (base_topic != NULL && strlen(base_topic) > 31) {
        ESP_LOGE(TAG, "Base topic out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (user != NULL && strlen(user) > 31) {
        ESP_LOGE(TAG, "User out of range");
        return ESP_ERR_INVALID_ARG;
    }

    if (password != NULL && strlen(password) > 63) {
        ESP_LOGE(TAG, "Password out of range");
        return ESP_ERR_INVALID_ARG;
    }

    nvs_set_u8(nvs, NVS_ENABLED, enabled);

    nvs_set_str(nvs, NVS_SERVER, server);

    nvs_set_str(nvs, NVS_BASE_TOPIC, base_topic);

    nvs_set_str(nvs, NVS_USER, user);

    nvs_set_str(nvs, NVS_PASSWORD, password);

    nvs_set_u16(nvs, NVS_PERIODICITY, _periodicity);
    periodicity = _periodicity;

    nvs_commit(nvs);

    if (enabled) {
        client_start();
    } else {
        client_stop();
    }

    return ESP_OK;
}

bool mqtt_get_enabled(void)
{
    uint8_t value = false;
    nvs_get_u8(nvs, NVS_ENABLED, &value);
    return value;
}

void mqtt_get_server(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_SERVER, value, &len);
}

void mqtt_get_base_topic(char* value)
{
    size_t len = 32;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_BASE_TOPIC, value, &len);
}

void mqtt_get_user(char* value)
{
    size_t len = 32;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_USER, value, &len);
}

void mqtt_get_password(char* value)
{
    size_t len = 64;
    value[0] = '\0';
    nvs_get_str(nvs, NVS_PASSWORD, value, &len);
}

uint16_t mqtt_get_periodicity(void)
{
    return periodicity;
}