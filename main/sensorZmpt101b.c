#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "adc_app.h"
#include "sensorZmpt101b.h"

const static char *TAG = "SENSOR_ZMPT101B LIBRARY";
uint8_t channelZmpt;

/*===================== Parameter Sensor Hall 50 =====================*/

static int offsetZmpt = 1000;     //offset do sensor
static int diffZmpt = 10;         //Queda de tensão
static int attenZmpt = 40;        //40(V/V) 

void* sensorZmpt101b_check(void){
    void *adc_handle = adc_check();
    return adc_handle;
}

void sensorZmpt101b_config(uint8_t _channel, void *adc_handle){
    channelZmpt = _channel;
    adc_config(channelZmpt, adc_handle);
}

float sensorZmpt101b_read(void){
    int voltage_in;
    float voltage_out;
    adc_read(&voltage_in, channelZmpt);
    voltage_out = (float)((voltage_in - offsetZmpt)*attenZmpt + diffZmpt)/1000;
    if(voltage_out > 50.0){
        ESP_LOGE(TAG, "Erro na leitura da voltage_out. %.1fA Acima do limite de 50A", voltage_out);
    }
    return voltage_out;
}

void sensorZmpt101b_deinit(void){
    adc_deinit();
}