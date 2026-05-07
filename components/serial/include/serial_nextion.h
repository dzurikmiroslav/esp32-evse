#ifndef SERIAL_NEXTION_H_
#define SERIAL_NEXTION_H_

#include <esp_err.h>
#include <unistd.h>

#define SERIAL_NEXTION_MODEL_MAX_SIZE    32
#define SERIAL_NEXTION_UPLOAD_BATCH_SIZE 4096

typedef struct {
    char model[SERIAL_NEXTION_MODEL_MAX_SIZE];
    bool touch : 1;
    uint32_t flash_size;
} serial_nextion_info_t;

/**
 * @brief Get nextion info
 *
 * @param info
 * @return esp_err_t
 */
esp_err_t serial_nextion_get_info(serial_nextion_info_t *info);

/**
 * @brief Start nextion upload, if fail not need call serial_nextion_upload_end
 *
 * @param file_size
 * @param baud_rate
 * @return esp_err_t
 */
esp_err_t serial_nextion_upload_begin(size_t file_size, uint32_t baud_rate);

/**
 * @brief Upload chunk, if fail not need call serial_nextion_upload_end
 *
 * @param data
 * @param len
 * @param skip_to
 * @return esp_err_t
 */
esp_err_t serial_nextion_upload_write(char *data, uint16_t len, size_t *skip_to);

/**
 * @brief Finish upload
 *
 * @return esp_err_t
 */
esp_err_t serial_nextion_upload_end(void);

#endif /* SERIAL_NEXTION_H_ */
