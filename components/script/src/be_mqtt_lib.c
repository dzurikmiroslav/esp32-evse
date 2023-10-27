#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "be_object.h"
#include "be_gc.h"
#include "be_vm.h"
#include "be_exec.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "sys/queue.h"

#include "script_utils.h"

static const char* TAG = "be_mqtt";

typedef struct sub_topic_s {
    SLIST_ENTRY(sub_topic_s) entries;
    bstring value;
} sub_topic_t;

typedef struct
{
    esp_mqtt_client_handle_t client;
    bvalue* connect_cb;
    SLIST_HEAD(sub_topics_head, sub_topic_s) sub_topics;
} mqtt_ctx_t;

static void event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    mqtt_ctx_t* ctx = handler_args;

    esp_mqtt_event_handle_t event = event_data;
    char topic[48];
    char data[256];

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        // vTaskResume(client_task);
        // subcribe_topics();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        break;
    case MQTT_EVENT_DATA:
        // memset(topic, 0, sizeof(topic));
        // strncpy(topic, event->topic, MIN(event->topic_len, sizeof(topic) - 1));
        // memset(data, 0, sizeof(data));
        // strncpy(data, event->data, MIN(event->data_len, sizeof(data) - 1));
        // handle_message(topic, data);
        break;
    default:
        break;
    }
}

static int m_init(bvm* vm)
{
    int argc = be_top(vm);
    if ((argc == 2 && be_isstring(vm, 2)) ||
        (argc == 3 && be_isstring(vm, 2) && be_isstring(vm, 3)) ||
        (argc == 4 && be_isstring(vm, 2) && be_isstring(vm, 3) && be_isstring(vm, 4))) {
        esp_mqtt_client_config_t cfg = { 0 };
        cfg.broker.address.uri = be_tostring(vm, 2);
        if (argc > 2) {
            cfg.credentials.username = be_tostring(vm, 3);
        }
        if (argc > 3) {
            cfg.credentials.authentication.password = be_tostring(vm, 4);
        }

        mqtt_ctx_t* ctx = (mqtt_ctx_t*)malloc(sizeof(mqtt_ctx_t));
        ctx->client = esp_mqtt_client_init(&cfg);
        SLIST_INIT(&ctx->sub_topics);
        be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
        be_setmember(vm, 1, "_ctx");

        be_return(vm);
    }
    be_raise(vm, "type_error", NULL);
    be_return_nil(vm);
}

static int m_connect(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 2 && be_isfunction(vm, 2)) {
        be_getmember(vm, 1, "_ctx");
        mqtt_ctx_t* ctx = (mqtt_ctx_t*)be_tocomptr(vm, -1);

        bvalue* cb = be_indexof(vm, 2);
        if (be_isgcobj(cb)) {
            be_gc_fix_set(vm, cb->v.gc, btrue);
        }
        ctx->connect_cb = cb;

        if (esp_mqtt_client_register_event(ctx->client, ESP_EVENT_ANY_ID, event_handler, ctx) != ESP_OK) {
            be_raise(vm, "mqtt_error", "cant register handler");
        }
        if (esp_mqtt_client_start(ctx->client) != ESP_OK) {
            be_raise(vm, "mqtt_error", "cant start");
            ESP_LOGW(TAG, "Cant start");
        }
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

/* @const_object_info_begin
class be_class_mqtt (scope: global, name: mqtt) {
    _ctx, var

    init, func(m_init)
    connect, func(m_connect)
}
@const_object_info_end */
#include "../generate/be_fixed_be_class_mqtt.h"