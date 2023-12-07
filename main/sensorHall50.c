#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "adc_app.h"
#include "sensorHall50.h"

const static char *TAG = "SENSOR_HALL50 LIBRARY";

/*===================== Parameter Sensor Hall 50 =====================*/

static int offset = 1000;     //offset do sensor
static int diff = 10;         //Queda de tensão
static int sensitivity = 40;  //40(mV/A) 
uint8_t channelHall;

void* sensorHall50_check(void){
    void *adc_handle = adc_check();
    return adc_handle;
}

void sensorHall50_config(uint8_t _channel, void *adc_handle){
    channelHall = _channel;
    adc_config(channelHall, adc_handle);
}

float sensorHall50_read(void){
    int voltage;
    float corrente;
    adc_read(&voltage, channelHall);
    corrente = (float)(voltage - offset)/sensitivity + diff;
    if(corrente > 50.0){
        ESP_LOGE(TAG, "Erro na leitura da corrente. %.1fA Acima do limite de 50A", corrente);
    }
    return corrente;
}

void sensorHall50_deinit(void){
    adc_deinit();
}