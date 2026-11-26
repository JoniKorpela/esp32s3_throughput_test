idf_component_register(SRCS "RX_access_point.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event esp_netif nvs_flash console iperf-cmd)
