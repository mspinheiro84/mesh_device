#ifndef __MESH_APP_H__
#define __MESH_APP_H__

esp_ip4_addr_t mesh_app_get_ip(void);
esp_err_t mesh_check_cmd_app(uint8_t cmd, uint8_t *data);
void mesh_send_app(int flagTo, uint8_t *data_to_send, uint16_t size);
void mesh_app_got_ip(void);
void mesh_app_type_root(bool isRoot);
void mesh_app(char ssid[], char pass[]);

#endif //__MESH_APP_H__