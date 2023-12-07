#ifndef __SENSORZMPT101B_H__
#define __SENSORZMPT101B_H__

void sensorZmpt101b_config(uint8_t _channel, void* adc_handle);
float sensorZmpt101b_read(void);
void sensorZmpt101b_deinit(void);
void* sensorZmpt101b_check(void);

#endif //__SENSORZMPT101B_H__
