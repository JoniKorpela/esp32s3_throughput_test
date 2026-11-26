idf_component_register(SRCS "TX_station.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event esp_netif nvs_flash console iperf-cmd)
