#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_event.h"
#include "mqtt_client.h"
#include "nvs.h"

#include "mqtt.h"
#include "board_config.h"
#include "json_utils.h"
#include "wifi.h"

#define NVS_NAMESPACE       "evse_mqtt"
#define NVS_ENABLED         "enabled"
#define NVS_SERVER          "server"
#define NVS_BASE_TOPIC      "base_topic"
#define NVS_USER            "user"
#define NVS_PASSWORD        "password"
#define NVS_PERIODICITY     "periodicity"

static const char* TAG = "mqtt";

static nvs_handle nvs;

static SemaphoreHandle_t mutex;

static esp_mqtt_client_handle_t client = NULL;

EventGroupHandle_t mqtt_event_group;

static uint16_t periodicity;

static uint16_t counter;

static void subcribe_topics(void)
{
    char topic[48];

    mqtt_get_base_topic(topic);
    strcat(topic, "/request/#");
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

        if (strcmp(sub_topic, "/enable") == 0) {
            cJSON* root = cJSON_Parse(data);
            if (cJSON_IsTrue(root)) {
                evse_enable(EVSE_DISABLE_BIT_USER);
            }
            if (cJSON_IsFalse(root)) {
                evse_disable(EVSE_DISABLE_BIT_USER);
            }
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/config/evse") == 0) {
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
        } else if (strcmp(sub_topic, "/request/boardConfig") == 0) {
            cJSON* root = json_get_board_config();
            publish_message("/response/boardConfig", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/request/info") == 0) {
            cJSON* root = json_get_info();
            publish_message("/response/info", root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/evse") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_evse_config(root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/wifi") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_wifi_config(root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/mqtt") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_mqtt_config(root);
            cJSON_Delete(root);
        } else if (strcmp(sub_topic, "/set/config/tcpLogger") == 0) {
            cJSON* root = cJSON_Parse(data);
            json_set_tcp_logger_config(root);
            cJSON_Delete(root);
        }
    }
}

static void mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    char topic[48];
    char data[256];

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        xEventGroupClearBits(mqtt_event_group, MQTT_DISCONNECTED_BIT);
        xEventGroupSetBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        subcribe_topics();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        xEventGroupClearBits(mqtt_event_group, MQTT_CONNECTED_BIT);
        xEventGroupSetBits(mqtt_event_group, MQTT_DISCONNECTED_BIT);
        break;
    case MQTT_EVENT_DATA:
        memset(topic, 0, sizeof(topic));
        strncpy(topic, event->topic, MIN(event->topic_len, sizeof(topic) - 1));
        memset(data, 0, sizeof(data));
        strncpy(data, event->data, MIN(event->data_len, sizeof(data) - 1));
        ESP_LOGI(TAG, "topic %s", topic);
        ESP_LOGI(TAG, "data %s", data);
        handle_message(topic, data);
        break;
    default:
        break;
    }
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    mqtt_event_handler_cb(event_data);
}

static void try_start(void)
{
    if (mqtt_get_enabled()) {
        char server[64];
        char user[32];
        char password[64];

        mqtt_get_server(server);
        mqtt_get_user(user);
        mqtt_get_password(password);
        periodicity = mqtt_get_periodicity();
        counter = 0;

        esp_mqtt_client_config_t mqtt_cfg = {
                .uri = server,
                .username = user,
                .password = password
        };
        client = esp_mqtt_client_init(&mqtt_cfg);
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
        esp_mqtt_client_start(client);
    }
}

void mqtt_init(void)
{
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs));

    mqtt_event_group = xEventGroupCreate();
    mutex = xSemaphoreCreateMutex();

    try_start();
}

void mqtt_set_config(bool enabled, const char* server, const char* base_topic, const char* user, const char* password, uint16_t periodicity)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    nvs_set_u8(nvs, NVS_ENABLED, enabled);
    if (server != NULL) {
        nvs_set_str(nvs, NVS_SERVER, server);
    }
    if (base_topic != NULL) {
        nvs_set_str(nvs, NVS_BASE_TOPIC, base_topic);
    }
    if (user != NULL) {
        nvs_set_str(nvs, NVS_USER, user);
    }
    if (password != NULL) {
        nvs_set_str(nvs, NVS_PASSWORD, password);
    }
    nvs_set_u16(nvs, NVS_PERIODICITY, periodicity);

    nvs_commit(nvs);

    if (client != NULL) {
        esp_mqtt_client_destroy(client);
    }
    try_start();

    xSemaphoreGive(mutex);
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
    uint16_t value = 30;
    nvs_get_u16(nvs, NVS_PERIODICITY, &value);
    return value;
}

void mqtt_process(void)
{
    xSemaphoreTake(mutex, portMAX_DELAY);

    if (client != NULL && periodicity > 0) {
        if (++counter == periodicity) {
            counter = 0;
            if (xEventGroupGetBits(mqtt_event_group) & MQTT_CONNECTED_BIT) {
                cJSON* root = json_get_state();
                publish_message("/state", root);
                cJSON_Delete(root);
            }
        }
    }

    xSemaphoreGive(mutex);
}
