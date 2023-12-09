#ifndef NVS_APP_H
#define NVS_APP_H

bool nvs_app_get(char *key, void *value, char tipo);
void nvs_app_set(char *key, void *value, char tipo);
void nvs_app_clear(char *key);

#endif //NVS_APP
