#include <stdio.h>
#include <string.h>

/*includes FreeRTOS*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/*include do ESP32*/
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "wifi.h"
#include "http_server.h"

static const char *TAG = "MAIN";
static bool credencial_wifi = false;
static char ssid[20], pass[20];

void http_server_receive_post(int tam, char *data){
    int pos;
    char *aux;
    ESP_LOGI(TAG, "=========== RECEIVED DATA MAIN ==========");
    ESP_LOGI(TAG, "%.*s", tam, data);
    ESP_LOGI(TAG, "====================================");
    
    // extraindo SSID
    aux = strchr(data, ':') +2;
    pos = strcspn(aux, "\"");
    memcpy(ssid, aux, pos);
    ssid[pos] = '\0';

    // extraindo PASS
    aux = strchr(aux, ':') + 2;
    pos = strcspn(aux, "\"");
    memcpy(pass, aux, pos);
    pass[pos] = '\0';

    ESP_LOGI(TAG, "=========== SSID e PASS ==========");
    ESP_LOGI(TAG, "SSID:%s", ssid);
    ESP_LOGI(TAG, "SSID:%s", pass);
    credencial_wifi = true;
}

void app_main(void)
{
    /*Conexão com WIFI*/
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // wifi_init_sta("AndroidAP", pass);

    wifi_init_softap();
    start_http_server();

    while (1){
        vTaskDelay(pdMS_TO_TICKS(1000));
        if(credencial_wifi){
          ESP_LOGW(TAG, "Desativando o wifi");
          wifi_stop();
          break;
        }
    }
    wifi_init_sta(ssid, pass);
}
