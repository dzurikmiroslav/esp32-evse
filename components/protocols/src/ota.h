#ifndef OTA_H
#define OTA_H

#include <esp_err.h>
#include <stdbool.h>
#include <sys/queue.h>

#define OTA_CHANNEL_SIZE 32
#define OTA_VERSION_SIZE 32

void ota_init(void);

/**
 * @brief OTA channel entry
 *
 */
typedef struct ota_channel_entry_s {
    char* channel;
    SLIST_ENTRY(ota_channel_entry_s) entries;
} ota_channel_entry_t;

/**
 * @brief OTA channel list
 *
 * @return typedef
 */
typedef SLIST_HEAD(ota_channel_list_s, ota_channel_entry_s) ota_channel_list_t;

/**
 * @brief Get OTA channel list
 *
 * @return ota_channel_list_t*
 */
ota_channel_list_t* ota_get_channel_list(void);

/**
 * @brief Free OTA channel list
 *
 * @param list
 */
void ota_channel_list_free(ota_channel_list_t* list);

void ota_get_channel(char* value);

void ota_set_channel(const char* value);

esp_err_t ota_get_available(char** version, char** path);

void schedule_restart(void);

#endif /* OTA_H */