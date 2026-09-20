idf_component_register(
    SRCS "main.cpp" "display.c" "joke_api.c" "battery.c" "ui.c" "wifi_sta.c" "time_sync.c" "board_power.c"
         "epaper_port.c" "GUI_Paint.c"
         "Fonts/font8.cpp" "Fonts/font12.cpp" "Fonts/font16.cpp"
         "Fonts/font20.cpp" "Fonts/font24.cpp"
    INCLUDE_DIRS "." "Fonts"
    REQUIRES driver esp_timer esp_wifi esp_netif esp_event nvs_flash
             esp_http_client esp-tls esp_adc
)
