#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/*includes FreeRTOS*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

/*includes do ESP32*/
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

/*includes do Projeto*/
#include "wifi_app.h"
#include "http_server.h"
#include "mesh_app.h"
#include "mqtt_app.h"
#include "nvs_app.h"
#include "ota_app.h"
#include "sensorZmpt101b.h"
#include "sensorHall50.h"

/*******************************************************
 *                Constants
 *******************************************************/

static const char *TAG = "MAIN";
static bool credencial_wifi = false;
static char ssid[] = "MESHSSID";
static char pass[] = "MESHPASSWORD";
static esp_ip4_addr_t s_current_ip;
static char addrMac[6*3+1]; // MAC addr size + terminator
static char *serialNumber;
TaskHandle_t xReadCorrenteHandle;

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
void mesh_app_disconnected(void){}


static void readCorrente(void *pvParameters){
    float corrente = 0, tensao = 0;
    sensorHall50_config(6, NULL);
    sensorZmpt101b_config(7, sensorHall50_check());

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, (TickType_t) portMAX_DELAY);
        corrente =  sensorHall50_read();
        tensao = sensorZmpt101b_read();
        ESP_LOGW(TAG, "Leitura realizada %.1fA, %.1fV", corrente, tensao);
    }
    
}

static void readS(TimerHandle_t xReadHandle){
    xTaskNotifyGive(xReadCorrenteHandle);
}

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
        char dataMac[3*6+1];
        sprintf(dataMac, MACSTR, MAC2STR(data));
        if (strcmp(dataMac, addrMac)){
            ESP_LOGW(TAG, "Keypressed detected on node: "
                MACSTR, MAC2STR(data));
            return ESP_OK;
        }
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

void mesh_app_got_ip(void){
    esp_mesh_comm_mqtt_task_start();
}

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
    char topic[33];
    char payload[44];
    static bool old_level = true;
    bool new_level;
    uint8_t *my_mac = (esp_mesh_is_root()) ? mesh_netif_get_ap_mac() : mesh_netif_get_station_mac();
    // snprintf(addrMac, sizeof(addrMac),MACSTR, MAC2STR(my_mac));
    sprintf(payload, "layer:%d IP:" IPSTR, esp_mesh_get_layer(), IP2STR(&s_current_ip));
    sprintf(topic, "mesh/%.*s/toDevice", 18, addrMac);

    uint8_t data_to_send[6+1] = { CMD_KEYPRESSED, };
    memcpy(data_to_send + 1, my_mac, 6);
    while (1) {
        new_level = gpio_get_level(BUTTON_GPIO);
        if (!new_level && old_level) {
            ESP_LOGW(TAG, "Key pressed!");
            /*mensage via mqtt*/
            ESP_LOGI(TAG, "Tried to publish in %s", topic);
            mqtt_app_publish(topic, payload);

            /*mensage via mesh*/
            mesh_send_app(-1, data_to_send, 7);
            
            // do_firmware_upgrade();

        }
        old_level = new_level;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

esp_err_t esp_mesh_comm_mqtt_task_start(void)
{
    TimerHandle_t xReadHandle;
    char *payload;
    char *topic;
    mqtt_app_start();
    s_current_ip = mesh_app_get_ip();    
    uint8_t *my_mac = (esp_mesh_is_root()) ? mesh_netif_get_ap_mac() : mesh_netif_get_station_mac();
    snprintf(addrMac, sizeof(addrMac),MACSTR, MAC2STR(my_mac));
    // asprintf(&payload, "layer:%d IP:" IPSTR, esp_mesh_get_layer(), IP2STR(&s_current_ip));
    asprintf(&payload, "{\"t\":%.*s,\"sn\":%s}", 18, addrMac, serialNumber);
    asprintf(&topic, "mesh/%.*s/toCloud", 18, addrMac);
    ESP_LOGI(TAG, "Tried to publish %s", payload);
    mqtt_app_publish(topic, payload);
    free(payload);
    free(topic);
    
    // if (esp_mesh_is_root()) {
    //     mesh_send_app(-1, NULL, 0);
    // }

    xTaskCreate(check_button, "check button task", 3072, NULL, 5, NULL);
    // xTaskCreate(timeNow, "TIME_OCLOCK", 2048, NULL, 1, NULL);
    xTaskCreate(readCorrente, "READ_CORRENTE", 2048, NULL, tskIDLE_PRIORITY, &xReadCorrenteHandle);
    xReadHandle = xTimerCreate("READ", pdMS_TO_TICKS(5000), pdTRUE, NULL, &readS);


    if (xReadHandle != NULL){
        xTimerStart(xReadHandle, 0);
    } else {
        ESP_LOGE(TAG, "Não foi possivel criar a taskTimer");
    }
    return ESP_OK;
}

void clear_credencial_wifi(){
    nvs_app_clear("ssid");
    nvs_app_clear("pass");
}

// void registro_serial(){
//     nvs_app_set("sn", "SW0000000002");
// }

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

    // clear_credencial_wifi();
    // registro_serial();
    serialNumber = (char *)calloc(13, sizeof(char));
    nvs_app_get("sn", serialNumber);
    /*check se botão esta pressionado e */
    if (check_button_root()){
        if (!nvs_app_get("ssid", ssid) || !nvs_app_get("pass", pass)){
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
