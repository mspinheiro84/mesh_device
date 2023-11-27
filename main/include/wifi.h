#ifndef __WIFI_H__
#define __WIFI_H__

void wifi_init_softap(void);
void wifi_init_sta(char *ssid, char *pass);
void wifi_stop(void);

#endif //WIFI_H
