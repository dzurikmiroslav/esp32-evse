#include "component_params.h"

#include <esp_log.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "yaml.h"

#define PARAMS_YAML     "/usr/lua/params.yaml"
#define PARAMS_TMP_YAML "/usr/lua/params_tmp.yaml"
#define BUF_SIZE        256

#define LOG_PARSER_PROBLEM(parser) ESP_LOGW(TAG, "Parsing error: %s (line: %zu column: %zu)", parser.problem, parser.problem_mark.line, parser.problem_mark.column)

static const char* TAG = "component_params";

static component_param_list_t* yaml_file_read(FILE* src, const char* key)
{
    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        ESP_LOGE(TAG, "Cant initialize yaml parser");
        return NULL;
    }

    component_param_list_t* list = (component_param_list_t*)malloc(sizeof(component_param_list_t));
    SLIST_INIT(list);

    yaml_parser_set_input_file(&parser, src);

    int level = 0;
    bool match = false;
    bool done = false;
    char* entry_key = NULL;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            LOG_PARSER_PROBLEM(parser);
            goto error;
        }

        switch (event.type) {
        case YAML_SCALAR_EVENT:
            if (level == 1) {
                match = strcmp(key, (char*)event.data.scalar.value) == 0;
            }

            if (level == 2 && match) {
                if (!entry_key) {
                    entry_key = strdup((char*)event.data.scalar.value);
                } else {
                    component_param_entry_t* entry = (component_param_entry_t*)malloc(sizeof(component_param_entry_t));
                    entry->key = entry_key;
                    entry->value = strdup((char*)event.data.scalar.value);
                    SLIST_INSERT_HEAD(list, entry, entries);
                    entry_key = NULL;
                }
            }

            break;
        case YAML_MAPPING_START_EVENT:
            level++;
            break;
        case YAML_MAPPING_END_EVENT:
            level--;
            break;
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        default:
            break;
        }

        if (event.type != YAML_SCALAR_EVENT && entry_key) {
            free((void*)entry_key);
            entry_key = NULL;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);

    return list;

error:
    ESP_LOGE(TAG, "Error read");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    if (entry_key) free((void*)entry_key);
    component_params_free(list);

    return NULL;
}

component_param_list_t* component_params_read(const char* component)
{
    component_param_list_t* list = NULL;

    FILE* src = fopen(PARAMS_YAML, "r");
    if (src) {
        list = yaml_file_read(src, component);
        fclose(src);
    }

    return list;
}

static void yaml_file_copy(FILE* dst, FILE* src)
{
    char buf[BUF_SIZE];
    size_t count;

    while ((count = fread(buf, 1, BUF_SIZE, src)) > 0) {
        fwrite(buf, 1, count, dst);
    }
}

static void yaml_file_copy_omit(FILE* dst, FILE* src, const char* key)
{
    yaml_parser_t parser;
    yaml_emitter_t emitter;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        ESP_LOGE(TAG, "Cant initialize yaml parser");
        return;
    }
    if (!yaml_emitter_initialize(&emitter)) {
        ESP_LOGE(TAG, "Cant initialize yaml emitter");
        yaml_parser_delete(&parser);
        return;
    }

    yaml_parser_set_input_file(&parser, src);
    yaml_emitter_set_output_file(&emitter, dst);
    //    yaml_emitter_set_unicode(&emitter, 1);

    int level = 0;
    bool skip = false;
    bool done = false;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            LOG_PARSER_PROBLEM(parser);
            goto error;
        }

        switch (event.type) {
        case YAML_SCALAR_EVENT:
            if (level == 1) {
                skip = strcmp(key, (char*)event.data.scalar.value) == 0;
            }
            break;
        case YAML_MAPPING_START_EVENT:
            level++;
            break;
        case YAML_MAPPING_END_EVENT:
            level--;
            break;
        case YAML_STREAM_END_EVENT:
            done = true;
            break;
        default:
            break;
        }

        if (skip) {
            yaml_event_delete(&event);
        } else {
            if (!yaml_emitter_emit(&emitter, &event)) {
                ESP_LOGW(TAG, "Emmiting error");
                goto error;
            }
        }
    }

    yaml_parser_delete(&parser);
    yaml_emitter_delete(&emitter);
    return;

error:
    ESP_LOGE(TAG, "Error copy with omit");
    yaml_event_delete(&event);
    yaml_parser_delete(&parser);
    yaml_emitter_delete(&emitter);
    return;
}

void yaml_file_append(FILE* dst, const char* key, component_param_list_t* list)
{
    yaml_emitter_t emitter;
    yaml_event_t event;

    if (!yaml_emitter_initialize(&emitter)) {
        ESP_LOGE(TAG, "Cant initialize yaml emitter");
        return;
    }

    yaml_emitter_set_output_file(&emitter, dst);

    yaml_stream_start_event_initialize(&event, YAML_UTF8_ENCODING);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_document_start_event_initialize(&event, NULL, NULL, NULL, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)key, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_mapping_start_event_initialize(&event, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    component_param_entry_t* entry;
    SLIST_FOREACH (entry, list, entries) {
        yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)entry->key, -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
        if (!yaml_emitter_emit(&emitter, &event)) goto error;

        yaml_scalar_event_initialize(&event, NULL, NULL, (yaml_char_t*)(entry->value ? entry->value : ""), -1, 1, 1, YAML_PLAIN_SCALAR_STYLE);
        if (!yaml_emitter_emit(&emitter, &event)) goto error;
    }

    yaml_mapping_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_mapping_end_event_initialize(&event);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_document_end_event_initialize(&event, 1);
    if (!yaml_emitter_emit(&emitter, &event)) goto error;

    yaml_emitter_delete(&emitter);
    return;

error:
    ESP_LOGE(TAG, "Error append");
    yaml_emitter_delete(&emitter);
    return;
}

void component_params_write(const char* component, component_param_list_t* list)
{
    FILE* file = fopen(PARAMS_YAML, "r");
    if (file) {  // some yaml exists
        FILE* src = file;
        FILE* dst = fopen(PARAMS_TMP_YAML, "w");
        yaml_file_copy_omit(dst, src, component);
        fclose(src);
        fclose(dst);

        src = fopen(PARAMS_TMP_YAML, "r");
        dst = fopen(PARAMS_YAML, "w");
        yaml_file_copy(dst, src);
        yaml_file_append(dst, component, list);
        fclose(src);
        fclose(dst);

        remove(PARAMS_TMP_YAML);
    } else {  // no yaml exists yet
        FILE* dst = fopen(PARAMS_YAML, "w");
        yaml_file_append(dst, component, list);
        fclose(dst);
    }
}

void component_params_free(component_param_list_t* list)
{
    while (!SLIST_EMPTY(list)) {
        component_param_entry_t* item = SLIST_FIRST(list);

        SLIST_REMOVE_HEAD(list, entries);

        if (item->key) free((void*)item->key);
        if (item->value) free((void*)item->value);
        free((void*)item);
    }

    free((void*)list);
}