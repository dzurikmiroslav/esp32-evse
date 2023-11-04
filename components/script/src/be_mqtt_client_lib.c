#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "be_object.h"
#include "be_gc.h"
#include "be_vm.h"
#include "be_exec.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "script_utils.h"

static const char* TAG = "be_mqtt_client";

typedef struct
{
    esp_mqtt_client_handle_t client;
    bool connected;
    bvalue on_connect;
    bvalue on_message;
} mqtt_client_ctx_t;

static void event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data)
{
    mqtt_client_ctx_t* ctx = handler_args;
    esp_mqtt_event_handle_t event = event_data;

    //ESP_LOGI(TAG, "event_handler %d", (int)event_id);

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ctx->connected = true;
        if (var_isfunction(&ctx->on_connect)) {
            xSemaphoreTake(script_mutex, portMAX_DELAY);
            bvalue* top = be_incrtop(script_vm);
            *top = ctx->on_connect;

            script_watchdog_reset();
            int ret = be_pcall(script_vm, 0);
            script_watchdog_disable();
            script_handle_result(script_vm, ret);

            be_pop(script_vm, 1);
            xSemaphoreGive(script_mutex);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ctx->connected = false;
        break;
    case MQTT_EVENT_DATA:
        if (var_isfunction(&ctx->on_message)) {
            xSemaphoreTake(script_mutex, portMAX_DELAY);
            bvalue* top = be_incrtop(script_vm);
            *top = ctx->on_message;
            be_pushnstring(script_vm, event->topic, event->topic_len);
            be_pushnstring(script_vm, event->data, event->data_len);

            script_watchdog_reset();
            int ret = be_pcall(script_vm, 2);
            script_watchdog_disable();
            script_handle_result(script_vm, ret);

            be_pop(script_vm, 3);
            xSemaphoreGive(script_mutex);
        }
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

        esp_mqtt_client_handle_t client = esp_mqtt_client_init(&cfg);
        if (client == NULL) {
            be_raise(vm, "mqtt_error", "invalid config");
            be_return_nil(vm);
        } else {
            mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)malloc(sizeof(mqtt_client_ctx_t));
            ctx->client = client;
            ctx->connected = false;
            var_setnil(&ctx->on_connect);
            var_setnil(&ctx->on_message);

            be_newcomobj(vm, ctx, &be_commonobj_destroy_generic);
            be_setmember(vm, 1, ".p");

            be_return(vm);
        }
    }
    be_raise(vm, "type_error", NULL);
    be_return_nil(vm);
}

static int m_deinit(bvm* vm)
{
    ESP_LOGI(TAG, "Deinit");

    be_getmember(vm, 1, ".p");
    mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)be_tocomptr(vm, -1);

    if (ctx->client != NULL) {
        esp_mqtt_client_destroy(ctx->client);
    }

    be_return_nil(vm);
}

static int m_connect(bvm* vm)
{
    int argc = be_top(vm);
    if ((argc == 1) ||
        (argc == 2 && be_isfunction(vm, 2))) {
        be_getmember(vm, 1, ".p");
        mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)be_tocomptr(vm, -1);

        if (argc > 1) {
            bvalue* cb = be_indexof(vm, 2);
            if (be_isgcobj(cb)) {
                be_gc_fix_set(vm, cb->v.gc, btrue);
            }
            ctx->on_connect = *cb;
        }

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

static int m_connected(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 1) {
        be_getmember(vm, 1, ".p");
        mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)be_tocomptr(vm, -1);

        be_pushbool(vm, ctx->connected);
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

static int m_publish(bvm* vm)
{
    int argc = be_top(vm);
    if ((argc == 3 && be_isstring(vm, 2) && be_isstring(vm, 3)) ||
        (argc == 4 && be_isstring(vm, 2) && be_isstring(vm, 3) && be_isint(vm, 4)) ||
        (argc == 5 && be_isstring(vm, 2) && be_isstring(vm, 3) && be_isint(vm, 4) && be_isint(vm, 5))) {
        char* topic = be_tostring(vm, 2);
        char* data = be_tostring(vm, 3);
        int qos = 1;
        int retry = 0;
        if (argc > 3) {
            qos = be_toint(vm, 4);
        }
        if (argc > 4) {
            retry = be_toint(vm, 5);
        }

        be_getmember(vm, 1, ".p");
        mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)be_tocomptr(vm, -1);

        if (ctx->connected) {
            int msg_id = esp_mqtt_client_publish(ctx->client, topic, data, 0, qos, retry);
            be_pushint(vm, msg_id);
        } else {
            be_pushint(vm, -1);
        }
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

static int m_set_on_message(bvm* vm)
{
    int argc = be_top(vm);
    if (argc == 2 && be_isfunction(vm, 2)) {
        be_getmember(vm, 1, ".p");
        mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)be_tocomptr(vm, -1);

        bvalue* cb = be_indexof(vm, 2);
        if (be_isgcobj(cb)) {
            be_gc_fix_set(vm, cb->v.gc, btrue);
        }
        ctx->on_message = *cb;
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

static int m_subscribe(bvm* vm)
{
    int argc = be_top(vm);
    if ((argc == 2 && be_isstring(vm, 2)) ||
        (argc == 3 && be_isstring(vm, 2) && be_isint(vm, 3))) {
        char* topic = be_tostring(vm, 2);
        int qos = 0;
        if (argc > 2) {
            qos = be_toint(vm, 3);
        }

        be_getmember(vm, 1, ".p");
        mqtt_client_ctx_t* ctx = (mqtt_client_ctx_t*)be_tocomptr(vm, -1);

        if (ctx->connected) {
            int sub_id = esp_mqtt_client_subscribe(ctx->client, topic, qos);
            be_pushint(vm, sub_id);
        } else {
            be_pushint(vm, -1);
        }
    } else {
        be_raise(vm, "type_error", NULL);
    }

    be_return(vm);
}

/* @const_object_info_begin
class be_class_mqtt_client (scope: global, name: mqtt_client) {
    .p, var

    init, func(m_init)
    deinit, func(m_deinit)

    connect, func(m_connect)
    connected, func(m_connected)
    publish, func(m_publish)
    set_on_message, func(m_set_on_message)
    subscribe, func(m_subscribe)
}
@const_object_info_end */
#include "../generate/be_fixed_be_class_mqtt_client.h"