#ifndef __MQTT_APP_H__
#define __MQTT_APP_H__

void mqtt_app_start(void);
void mqtt_app_publish(char* topic, char *publish_string);
void mqtt_app_subscribe(char* topic);
void mqtt_app_unsubscribe(char* topic);
void mqtt_app_event_connected(void); // trata conexão
void mqtt_app_event_disconnected(void); // trata desconexão
void mqtt_app_event_data(char *publish_string, int tam); // trata os dados recebidos

#endif //__MESH_APP_H__