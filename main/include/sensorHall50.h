#ifndef __SENSORHALL50_H__
#define __SENSORHALL50_H__

void sensorHall50_config(uint8_t _channel, void* adc_handle);
float sensorHall50_read(void);
void sensorHall50_deinit(void);
void* sensorHall50_check(void);

#endif //__SENSORHALL50_H__
