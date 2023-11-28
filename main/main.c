#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/*includes FreeRTOS*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

/*include do ESP32*/
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_mesh.h"
#include "mesh_netif.h"
// #include "nvs.h"

#include "wifi_app.h"
#include "http_server.h"
#include "mesh_app.h"
#include "mqtt_app.h"
#include "nvs_app.h"

/*******************************************************
 *                Constants
 *******************************************************/

static const char *TAG = "MAIN";
static bool credencial_wifi = false;
static char ssid[] = "MESHSSID";
static char pass[] = "MESHPASSWORD";
static bool is_running = true;

/*******************************************************
 *                Macros
 *******************************************************/

#define BUTTON_GPIO     13
#define LED_GPIO        2

// commands for internal mesh communication:
// <CMD> <PAYLOAD>, where CMD is one character, payload is variable dep. on command
#define CMD_KEYPRESSED 0x55
// CMD_KEYPRESSED: payload is always 6 bytes identifying address of node sending keypress event


/*******************************************************
 *                Declarações
 *******************************************************/
esp_err_t esp_mesh_comm_mqtt_task_start(void);

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

esp_err_t mesh_check_cmd_app(uint8_t cmd, uint8_t *data){
    if (cmd == CMD_KEYPRESSED) {
        ESP_LOGW(TAG, "Keypressed detected on node: "
            MACSTR, MAC2STR(data));
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

void mesh_app_got_ip(void){
    esp_mesh_comm_mqtt_task_start();
}

void mesh_app_disconnected(void){}


static void initialise_gpio(void)
{
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = BIT64(BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = BIT64(LED_GPIO);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

void inicialize_blink(bool root){
    ESP_LOGI(TAG, "device is %s", root ? "ROOT" : "NODE");
    for(int i=0; i<9; i++){
        gpio_set_level(LED_GPIO, i%2);
        vTaskDelay(pdMS_TO_TICKS(750));
    }
    gpio_set_level(LED_GPIO, root);
}

bool check_button_root(void){
    if (!gpio_get_level(BUTTON_GPIO)){
        mesh_app_type_root(true);
        inicialize_blink(true);
        return true;
    }
    inicialize_blink(false);
    return false;
}

static void check_button(void* args)
{
    static bool old_level = true;
    bool new_level;
    bool run_check_button = true;
    while (run_check_button) {
        new_level = gpio_get_level(BUTTON_GPIO);
        if (!new_level && old_level) {
            if (!esp_mesh_is_root()) {
                ESP_LOGW(TAG, "Key pressed!");                
                uint8_t data_to_send[6+1] = { CMD_KEYPRESSED, };
                uint8_t *my_mac = mesh_netif_get_station_mac();
                memcpy(data_to_send + 1, my_mac, 6);
                char print[6*3+1]; // MAC addr size + terminator
                snprintf(print, sizeof(print),MACSTR, MAC2STR(my_mac));
                mqtt_app_publish("/topic/ip_mesh/key_pressed", print);                
                mesh_send_app(-1, data_to_send, 7);
            }
        }
        old_level = new_level;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void esp_mesh_mqtt_task(void *arg)
{
    is_running = true;
    char *print;
    mqtt_app_start();
    while (is_running) {
        esp_ip4_addr_t myIp = mesh_app_get_ip();
        asprintf(&print, "layer:%d IP:" IPSTR, esp_mesh_get_layer(), IP2STR(&myIp));
        ESP_LOGI(TAG, "Tried to publish %s", print);
        mqtt_app_publish("/topic/ip_mesh", print);
        free(print);
        if (esp_mesh_is_root()) {
            mesh_send_app(-1, NULL, 0);
        }
        vTaskDelay(2 * 3000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_comm_mqtt_task_start(void)
{
    xTaskCreate(esp_mesh_mqtt_task, "mqtt task", 3072, NULL, 5, NULL);
    xTaskCreate(check_button, "check button task", 3072, NULL, 5, NULL);
    return ESP_OK;
}

void clear_memory(){
    nvs_app_clear("ssid");
    nvs_app_clear("pass");
}


void app_main(void)
{
    initialise_gpio();
    /*Conexão com WIFI*/
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /*check se botão esta pressionado e */
    if (check_button_root()){
        
        if (!nvs_app_get("ssid", &ssid[0]) || !nvs_app_get("pass", &pass[0])){
            wifi_init_softap();
            start_http_server();

            while (1){
                vTaskDelay(pdMS_TO_TICKS(500));
                if(credencial_wifi){
                    wifi_stop();
                    nvs_app_set("ssid", ssid);
                    nvs_app_set("pass", pass);
                    break;
                }
            }
        }
    }
    mesh_app(ssid, pass);
}
