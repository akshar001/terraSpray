#ifndef MAIN_LIBRARY_TCP_H_
#define MAIN_LIBRARY_TCP_H_

#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"

void start_websocket_server(void);
extern EventGroupHandle_t event_group;
static const int PROCEED_STATE      = BIT0;       // Wait satte ended on APP setting and now proceed with  normal state machine
static const int DEMO_MODE_START    = BIT1;       // demo Mode API called from APP, serve a demo and wait fro reposne form FW
static const int DEMO_MODE_STOP     = BIT2;       // demo Mode API called from APP, serve a demo and wait fro reposne form FW
static const int WEBSOCKET_STATE    = BIT3;       // Websocket current_satte waiting event
static const int SET_TIME_STATE     = BIT4;       // Websocket current_satte waiting event
void update_scheduler(uint8_t index);

#endif /* MAIN_LIBRARY_TCP_H_ */
