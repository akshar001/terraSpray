set(COMPONENT_SRCS "main.c"
"osal/event.c"
"osal/memory.c"
"osal/mutex.c"
"osal/task.c"
"osal/times.c"
"hal/envs.c"
"hal/network.c"
"hal/system.c"
"config/config.c"
"sdk/wifi.c"
"sdk/WebSocket1.c"
"sdk/app.c"
"sdk/battery.c"
"sdk/websocket.c"
"sdk/websocket_server.c"
"file_server.c"
"sdk/ds1307.c"
"sdk/i2cdev.c"
EMBED_FILES "favicon.ico" "upload_script.html"
)

set(COMPONENT_ADD_INCLUDEDIRS 
"."
"./hal"
"./osal"
"./config"
"./sdk"
)

register_component(
"esp_websocket_client"
"wpa_supplicant")
