#ifndef OTA_H_
#define OTA_H_

#include <stdbool.h>

typedef struct {
    bool has_uptade: 1;
    char version[32];
} ota_check_result_t;

ota_check_result_t ota_check(void);

#endif /* OTA_H_ */
