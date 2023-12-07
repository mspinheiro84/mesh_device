#ifndef __ADC_APP_H__
#define __ADC_APP_H__

void adc_config(uint8_t _channel, void *_adc_handle);
uint8_t adc_read(int *voltage, uint8_t channel);
void adc_deinit(void);
void* adc_check(void);

#endif //__ADC_APP_H__
