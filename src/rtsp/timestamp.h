#pragma once

extern int timestamp_init(char* ip, unsigned int port_tx, unsigned int port_rx);
extern void timestamp_venc_finished(void);
extern void timestamp_send_finished(unsigned long frameNb);
extern void timestamp_deinit(void);