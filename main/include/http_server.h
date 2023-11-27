#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

void http_server_receive_post(int tam, char *data);
void start_http_server(void);
void stop_http_server(void);

#endif //HTTP_SERVER
