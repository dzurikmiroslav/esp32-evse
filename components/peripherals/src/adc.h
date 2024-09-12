#ifndef ADC_H_
#define ADC_H_

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_adc/adc_oneshot.h>

extern adc_oneshot_unit_handle_t adc_handle;

extern adc_cali_handle_t adc_cali_handle;

void adc_init(void);

#endif /* ADC_H_ */