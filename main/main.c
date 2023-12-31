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

/*******************************************************
 *                Macros
 *******************************************************/

#if CONFIG_HARDWARE_MODE_SWITCH
#define SWITCH_GPIO1    27
#define SWITCH_GPIO2    26
#define SWITCH_GPIO3    25
#define SWITCH_GPIO4    33
#define TAG_SWITCH      "s"

static uint8_t switchGpio[4] = {0,0,0,0};
static uint32_t timerSendSwitch = 10; //tempo em segundos

#else
#define SENSOR_GPIO1    6 // PIN34
#define SENSOR_GPIO2    7 // PIN35
#define TAG_VOLTAGE     "v"
#define TAG_AMPERE      "a"

#endif

#define TAG_NEWDEVICE   "nd"
#define BUTTON_GPIO     13
#define LED_GPIO        2 // 32

// commands for internal mesh communication:
// <CMD> <PAYLOAD>, where CMD is one character, payload is variable dep. on command
#define CMD_KEYPRESSED 0x55
// CMD_KEYPRESSED: payload is always 6 bytes identifying address of node sending keypress event
// #define CMD_KEYPRESSED 0x55
// // CMD_KEYPRESSED: payload is always 6 bytes identifying address of node sending keypress event


/*******************************************************
 *                Declarações
 *******************************************************/
esp_err_t esp_mesh_comm_mqtt_task_start(void);
void mesh_app_disconnected(void){}

char* extractJson(char *json, char *name)
{
    if ((json != NULL) && (name != NULL) && (json[0] == '{')){
        int pos, tam;
        char *aux;
        tam = strlen(json);
        aux = malloc(sizeof(char)*tam);
        memcpy(aux, json, tam);
        tam = strlen(name);
        while (1){
            aux = strchr(aux, '\"');
            aux++;
            pos = strcspn(aux+1, "\"")+1;
            if ((pos == tam )&&(!strncmp(aux, name, tam))){
                aux = aux+tam+3;
                pos = strcspn(aux, "\"");
                aux[pos] = '\0';
                return aux;
            }
            if(strcspn(aux, "}") == 0){
                ESP_LOGI(TAG, "Não encontrou");
                return NULL;
            }
        }
    }
    ESP_LOGI(TAG, "Parametro nulo");
    return NULL;
}


void mesh_send_root(char *data_to_send){
    char topic[33];
    sprintf(topic, "mesh/%.*s/toDevice", 18, addrMac);
    mqtt_app_publish(topic, (char*) data_to_send);
}


#if CONFIG_HARDWARE_MODE_SENSOR
TaskHandle_t xReadSensorHandle, xSendSensorHandle;

static void sendSensor(void *pvParameters)
{
    static time_t now;
    time(&now);
    //Set timezone to Brazil Standard Time
    setenv("TZ", "UTC+3", 1);
    tzset();
    while(1){
        static struct tm timeinfo;
        char hora[9];
        time(&now);
        localtime_r(&now, &timeinfo);
        // // Is time set? If not, tm_year will be (1970 - 1900).
        // if (timeinfo.tm_year < (2022 - 1900)) {
        //     ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        //     obtain_time();
        //     // update 'now' variable with current time
        //     time(&now);
        // }
        // strftime(data, sizeof(data), "%a %d/%m/%Y", &timeinfo);
        // printf("\nData: %s\n", data);

        ulTaskNotifyTake(pdTRUE, (TickType_t) portMAX_DELAY);

        strftime(hora, sizeof(hora), "%X", &timeinfo);
        printf("\nHorário no Brasil: %s\n\n", hora);

        nvs_stats_t nvs_stats;
        nvs_get_stats(NULL, &nvs_stats);
        printf("Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n",
                nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);            

        char payload[54];
        // char topic[33];
        // sprintf(topic, "mesh/%.*s/toDevice", 18, addrMac);
        char keyVoltagem[5] = TAG_VOLTAGE;
        char keyAmperes[5] = TAG_AMPERE;
        char keyCont[5] = "cont";
        int cont;
        nvs_app_get(keyCont, &cont, 'i');
        int aux = 0;
        nvs_app_set(keyCont, &aux, 'i');
        int tensao, corrente;
        ESP_LOGI(TAG, "Send mqtt lot!");
        for (int i = 1; i <= cont; i++){
            strcpy(keyVoltagem, TAG_VOLTAGE);
            strcpy(keyAmperes, TAG_AMPERE);
            __itoa(i, &keyVoltagem[1], 10);
            __itoa(i, &keyAmperes[1], 10);
            tensao = 0;
            corrente = 0;
            nvs_app_get(keyAmperes, &corrente, 'i');
            nvs_app_get(keyVoltagem, &tensao, 'i');
            sprintf(payload, "{\"sn\":\"%s\",\"s\":{\"%s\":%d,\"%s\":%d}}", serialNumber ,TAG_VOLTAGE, tensao, TAG_AMPERE, corrente);
            // mqtt_app_publish(topic, payload);
            mesh_send_app(1, &payload, strlen(payload));
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

static void readSensor(void *pvParameters)
{
    int corrente = 0, tensao = 0;
    sensorHall50_config(SENSOR_GPIO1, NULL);
    sensorZmpt101b_config(SENSOR_GPIO2, sensorHall50_check());
    int cont = 0;
    char keyCont[5] = "cont";
char keyVoltagem[5] = TAG_VOLTAGE;
    char keyAmperes[5] = TAG_AMPERE;

    while (1)
    {
        ulTaskNotifyTake(pdTRUE, (TickType_t) portMAX_DELAY);
        /*Leitura dos sensores*/
        corrente = (int) sensorHall50_read()*100;
        tensao = (int) sensorZmpt101b_read()*100;
        // ESP_LOGW(TAG, "Leitura realizada %dA, %dV", corrente, tensao);

        /*Gravação das leituras na memória: contador, leitura da corrente, leitura da tensão*/
        nvs_app_get(keyCont, &cont, 'i');
        cont++;
        __itoa(cont, &keyVoltagem[1], 10);
        __itoa(cont, &keyAmperes[1], 10);
        // ESP_LOGW(TAG, "Teste - %d", cont);
        nvs_app_set(keyCont, &cont, 'i');
        nvs_app_set(keyAmperes, &corrente, 'i');
        nvs_app_set(keyVoltagem, &tensao, 'i');
        
        if (cont>49){
            xTaskNotifyGive(xSendSensorHandle);
            // timeRTC();
        }
    }
}


static void readS(TimerHandle_t xReadHandle)
{
    xTaskNotifyGive(xReadSensorHandle);
}

#else

void setSwitch(char *dado){
    switchGpio[0] = dado[0]=='-' ? switchGpio[0] : (uint8_t)dado[0]&1;
    switchGpio[1] = dado[1]=='-' ? switchGpio[1] : (uint8_t)dado[1]&1;
    switchGpio[2] = dado[2]=='-' ? switchGpio[2] : (uint8_t)dado[2]&1;
    switchGpio[3] = dado[3]=='-' ? switchGpio[3] : (uint8_t)dado[3]&1;

    gpio_set_level(SWITCH_GPIO1, switchGpio[0]);
    gpio_set_level(SWITCH_GPIO2, switchGpio[1]);
    gpio_set_level(SWITCH_GPIO3, switchGpio[2]);
    gpio_set_level(SWITCH_GPIO4, switchGpio[3]);
    nvs_app_set(TAG_SWITCH, dado, 's');
}

static void sendSwitch(TimerHandle_t xSendHandle)
{
    char topic[33];
    char payload[54];
    char *dado;
    dado = (char *)calloc(4, sizeof(char));
    nvs_app_get(TAG_SWITCH, dado, 's');
    // ESP_LOGW(TAG, "dado salvo %s", dado);
    setSwitch(dado);
    // sprintf(topic, "mesh/%.*s/toDevice", 18, addrMac);
    sprintf(payload, "{\"sn\":\"%s\",\"%s\":\"%d%d%d%d\"}", serialNumber ,TAG_SWITCH, switchGpio[0], switchGpio[1], switchGpio[2], switchGpio[3]);
    // mqtt_app_publish(topic, payload);
    mesh_send_app(1, &payload, strlen(payload));
}
#endif //CONFIG_HARDWARE_MODE_SENSOR

void mesh_recv_app(char *data, uint16_t data_size){
    data[data_size] = '\0';
    // ESP_LOGW(TAG, "Tamanho da string:%d", data_size);
    // ESP_LOGW(TAG, "Chegou para extrair:%s", data);
    char* dest = extractJson(data, "sn");
    if (dest == NULL) return;
    if (!strcmp(dest, serialNumber)){
        // ESP_LOGW(TAG, "Chegou mesh: É para mim ;)");
        // ESP_LOGW(TAG, "%s", data);
        data[data_size] = '\0';
        char aux[data_size+1];
        strcpy(aux, data);
        char *dado;
#if CONFIG_HARDWARE_MODE_SWITCH
        dado = extractJson(aux, TAG_SWITCH);
        if (dado != NULL) {
            // ESP_LOGW(TAG, "Retorno %s", dado);
            setSwitch(dado);
            return;
        }
#endif
    } else {
        // ESP_LOGW(TAG, "Chegou mesh: É para outro ;)");
        if (mesh_app_is_root())
            mesh_send_root(data);
    }
}

void mqtt_app_event_data(char *publish_string, int tam)
{
    // ESP_LOGD(TAG, "%.*s", tam, publish_string);
    publish_string[tam] = '\0';
    char aux[tam+1];
    strcpy(aux, publish_string);
    // char *aux = malloc(sizeof(char)*tam);
    // memcpy(aux, publish_string, tam);
    // aux[tam] = '\0';
    char *dado;
    // ESP_LOGW(TAG, "Chegou mqtt - Mensagem:%s", aux);
    dado = extractJson(aux, "sn");
    if (dado != NULL){
        if (!strcmp(dado, serialNumber)){
            // ESP_LOGW(TAG, "Mensagem para mim :D");
#if CONFIG_HARDWARE_MODE_SWITCH
            dado = extractJson(aux, TAG_SWITCH);
            if (dado != NULL) {
                // ESP_LOGW(TAG, "Retorno %s", dado);
                setSwitch(dado);
                return;
            }
#endif
        } else {
            // ESP_LOGW(TAG, "Mensagem para outro :p");
            dado = extractJson(aux, TAG_NEWDEVICE);
            if (dado != NULL){
                ESP_LOGW(TAG, "Novo device à ser conectado, %s", dado);
                return;
            }
            mesh_send_app(-1, &aux, tam);
        }
    }


}

void http_server_receive_post(int tam, char *data)
{
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

esp_err_t mesh_check_cmd_app(uint8_t cmd, uint8_t *data)
{
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

void mesh_app_got_ip(bool root)
{
    if (root) mqtt_app_start();
    esp_mesh_comm_mqtt_task_start();
}

void mqtt_app_event_connected(void){
    char topic[33];
    uint8_t *my_mac = (esp_mesh_is_root()) ? mesh_netif_get_ap_mac() : mesh_netif_get_station_mac();
    snprintf(addrMac, sizeof(addrMac),MACSTR, MAC2STR(my_mac));
    sprintf(topic, "mesh/%.*s/toDevice", 18, addrMac);
    mqtt_app_subscribe(topic);
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

#if CONFIG_HARDWARE_MODE_SWITCH
    io_conf.pin_bit_mask = BIT64(SWITCH_GPIO1)|BIT64(SWITCH_GPIO2)|BIT64(SWITCH_GPIO3)|BIT64(SWITCH_GPIO4);
    gpio_config(&io_conf);
#endif
}

void inicialize_blink(bool root)
{
    ESP_LOGI(TAG, "device is %s", root ? "ROOT" : "NODE");
    for(int i=0; i<9; i++){
        gpio_set_level(LED_GPIO, i%2);
        vTaskDelay(pdMS_TO_TICKS(750));
    }
    gpio_set_level(LED_GPIO, root);
}

bool check_button_root(void)
{
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
    // char topic[33];
    static char payload[54];
    static bool old_level = true;
    bool new_level;
    uint8_t *my_mac = (esp_mesh_is_root()) ? mesh_netif_get_ap_mac() : mesh_netif_get_station_mac();
    // snprintf(addrMac, sizeof(addrMac),MACSTR, MAC2STR(my_mac));
    sprintf(payload, "{\"sn\":\"%s\",\"layer\":\"%d\", \"IP\":\"" IPSTR "\"}", serialNumber, esp_mesh_get_layer(), IP2STR(&s_current_ip));
    // sprintf(topic, "mesh/%.*s/toDevice", 18, addrMac);

    uint8_t data_to_send[6+1] = { CMD_KEYPRESSED, };
    memcpy(data_to_send + 1, my_mac, 6);
    while (1) {
        new_level = gpio_get_level(BUTTON_GPIO);
        if (!new_level && old_level) {
            ESP_LOGW(TAG, "Key pressed!");
            /*mensage via mqtt*/
            // ESP_LOGI(TAG, "Tried to publish in %s", topic);
            // mqtt_app_publish(topic, payload);

            ESP_LOGW(TAG, "%s", payload);
            ESP_LOGW(TAG, "Tamanho:%d", strlen(payload));
            mesh_send_app(1, &payload, strlen(payload));
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
    char payload[54];
    // char *topic;
    s_current_ip = mesh_app_get_ip();
    uint8_t *my_mac = (esp_mesh_is_root()) ? mesh_netif_get_ap_mac() : mesh_netif_get_station_mac();
    snprintf(addrMac, sizeof(addrMac),MACSTR, MAC2STR(my_mac));
    // asprintf(&payload, "layer:%d IP:" IPSTR, esp_mesh_get_layer(), IP2STR(&s_current_ip));
    sprintf(&payload, "{\"m\":\"%.*s\",\"sn\":\"%s\"}", 18, addrMac, serialNumber);
    // asprintf(&topic, "mesh/%.*s/toCloud", 18, addrMac);
    ESP_LOGI(TAG, "Tried to publish %s", payload);
    mesh_send_app(1, &payload, strlen(payload));
    // mqtt_app_publish(topic, payload);
    // mqtt_app_subscribe(topic);

#if CONFIG_HARDWARE_MODE_SWITCH
    ESP_LOGI(TAG, "Mode Switch.");
    char *dado;
    dado = (char *)calloc(4, sizeof(char));
    nvs_app_get(TAG_SWITCH, dado, 's');
    setSwitch(dado);
    // asprintf(&topic, "mesh/%.*s/toDevice", 18, addrMac);
    // mqtt_app_subscribe(topic);

    TimerHandle_t xSendHandle;
    xSendHandle = xTimerCreate("SEND", pdMS_TO_TICKS(1000*timerSendSwitch), pdTRUE, NULL, &sendSwitch);

    if (xSendHandle != NULL){
        xTimerStart(xSendHandle, 0);
    } else {
        ESP_LOGE(TAG, "Não foi possivel criar a taskTimer");
    }
#else
    ESP_LOGI(TAG, "Mode Sensor.");
#endif

    // free(topic);
    
    xTaskCreate(check_button, "check button task", 3072, NULL, 5, NULL);

#if CONFIG_HARDWARE_MODE_SENSOR
    TimerHandle_t xReadHandle;

    xTaskCreate(sendSensor, "SEND_SENSOR", 2048, NULL, tskIDLE_PRIORITY, &xSendSensorHandle);
    xTaskCreate(readSensor, "READ_SENSOR", 2048, NULL, tskIDLE_PRIORITY, &xReadSensorHandle);
    xReadHandle = xTimerCreate("READ", pdMS_TO_TICKS(5000), pdTRUE, NULL, &readS);

    if (xReadHandle != NULL){
        xTimerStart(xReadHandle, 0);
    } else {
        ESP_LOGE(TAG, "Não foi possivel criar a taskTimer");
    }

#endif // CONFIG_HARDWARE_MODE_SENSOR
    return ESP_OK;
}

void clear_credencial_wifi(void)
{
    nvs_app_clear("ssid");
    nvs_app_clear("pass");
}

// void registro_serial(){
//     nvs_app_set("sn", "SW0000000002", 's');
// }

void app_main(void)
{
    // nvs_flash_erase();
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
    nvs_app_get("sn", serialNumber, 's');

    /*check se botão esta pressionado e */
    if (check_button_root()){
        if (!nvs_app_get("ssid", ssid, 's') || !nvs_app_get("pass", pass, 's')){
            wifi_init_softap();
            start_http_server();

            while (1){
                vTaskDelay(pdMS_TO_TICKS(500));
                if(credencial_wifi){
                    wifi_stop();
                    nvs_app_set("ssid", ssid, 's');
                    nvs_app_set("pass", pass, 's');
                    break;
                }
            }
        }
    }
    mesh_app(ssid, pass);    
}
