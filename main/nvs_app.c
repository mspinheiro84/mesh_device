/* Non-Volatile Storage (NVS) Read and Write a Value - Example

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "NVS_APP LIBRARY";
static const char *nameSpace = "memoriaESP";


bool nvs_app_get(char *key, char *value)
{
    // Open
    size_t required_size;
    nvs_handle_t nvsHandle;
    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");
    esp_err_t err = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        // printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return false;
    } else {
        // printf("Done\n");

        // Read
        ESP_LOGI(TAG, "Reading %s from NVS ... ", key);
        err = nvs_get_str(nvsHandle, key, value, &required_size);
        value = (char *) calloc(required_size, sizeof(char));
        err = nvs_get_str(nvsHandle, key, value, &required_size);
        switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG, "Done. %s: %s", key, value);
                // printf("Done\n");
                // printf("Restart counter = %d\n", restart_counter);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGE(TAG, "The value is not initialized yet!\n");
                // printf("The value is not initialized yet!\n");
                break;
            default :
                ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
                // printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        // Close
        nvs_close(nvsHandle);
        if (err == ESP_OK){
            return true;    
        } else {
            return false;
        }
    }
}

void nvs_app_set(char *key, char *value)
{
    nvs_handle_t nvsHandle;
    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");
    esp_err_t err = nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        // printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    } else {

        // Write
        ESP_LOGI(TAG, "Updating %s in NVS %s ... ", key, value);
        // printf("Updating restart counter in NVS ... ");
        err = nvs_set_str(nvsHandle, key, value);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Committing updates in NVS ... ");
            err = nvs_commit(nvsHandle);
            if (err == ESP_OK){
                ESP_LOGI(TAG, "Done!");
                nvs_close(nvsHandle);
                return;
            }
        }
        ESP_LOGE(TAG, "Failed!");        
        // Close
        nvs_close(nvsHandle);
        // printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure changes are written
        // to flash storage. Implementations may write to storage at other times,
        // // but this is not guaranteed.
        // printf("Committing updates in NVS ... ");
    }
}

void nvs_app_clear(char *key){
    nvs_handle_t nvsHandle;
    nvs_open(nameSpace, NVS_READWRITE, &nvsHandle);
    nvs_erase_key(nvsHandle, key);
    nvs_close(nvsHandle);
    ESP_LOGW(TAG, "Apagando...");
}