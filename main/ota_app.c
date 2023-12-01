#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_err.h"
#include "esp_https_ota.h"


static const char *TAG = "OTA_APP LIBRARY";

/* Event handler for catching system events */
static void ota_event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                ESP_LOGI(TAG, "OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
                ESP_LOGI(TAG, "Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
                ESP_LOGI(TAG, "Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
                ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
                ESP_LOGI(TAG, "Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
                ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
                ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                ESP_LOGI(TAG, "OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
                ESP_LOGI(TAG, "OTA abort");
                break;
        }
    }
}

esp_err_t do_firmware_upgrade(void)
{
    esp_http_client_config_t config = {
        .url = "http://192.168.140.107/ftp/firmware/device_mesh.bin",
        // .cert_len = 0,
        .cert_pem = (char *)"server_cert_pem_start",
        // .username = "esp",
        // .password = "123456",
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    esp_err_t ret = esp_https_ota(&ota_config);
    ESP_LOGW(TAG, "%s", config.url);
    esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_event_handler, NULL);
    if (ret == ESP_OK) {
        ESP_LOGW(TAG, "Reiniciando....");
        esp_restart();
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}