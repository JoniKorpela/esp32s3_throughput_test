#include <stdio.h>
#include <stdlib.h>

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "iperf_cmd.h"

#define WIFI_SSID       "wifitesti"
#define WIFI_PASSWORD   "06080608"
#define WIFI_CHANNEL    6
#define WIFI_TX_POWER_QDBM 28 /* Quarter dBm units. 7 dBm; assumes at least 1 dB cable loss with 14 dBi antenna */

static const char *TAG = "rx_access_point";

static int txpower_command(int argc, char **argv)
{
    if (argc == 1) {
        int8_t power_qdbm;
        ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&power_qdbm));
        int applied_dbm = power_qdbm == 28 ? 7 : (power_qdbm == 20 ? 5 : 2);
        ESP_LOGI(TAG, "Maximum Wi-Fi TX power: %d dBm (driver value %d quarter-dBm)",
                 applied_dbm, power_qdbm);
        return 0;
    }
    if (argc != 2) {
        printf("Usage: txpower [2|3|4|5|6] (dBm; 6 request applies the 7 dBm radio step)\n");
        return 1;
    }
    char *end = NULL;
    long dbm = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || dbm < 2 || dbm > 6) {
        printf("Allowed requests: 2, 3, 4, 5, or 6 dBm.\n");
        return 1;
    }
    int8_t requested_qdbm = (dbm == 6) ? 28 : (int8_t)(dbm * 4);
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(requested_qdbm));
    int8_t applied_qdbm;
    ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&applied_qdbm));
    ESP_LOGI(TAG, "Requested %ld dBm; driver applied %d dBm", dbm,
             applied_qdbm == 28 ? 7 : (applied_qdbm == 20 ? 5 : 2));
    return 0;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Open AP ready: SSID='%s', channel=%d, address=192.168.4.1", WIFI_SSID, WIFI_CHANNEL);
        ESP_LOGI(TAG, "Start the iperf server at the serial prompt: iperf -s -i 1");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = event_data;
        ESP_LOGI(TAG, "Station joined, AID=%d", event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = event_data;
        ESP_LOGI(TAG, "Station left, AID=%d", event->aid);
    }
}

static void init_console(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "rx> ";
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usb_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb_config, &repl_config, &repl));
#else
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#endif
    ESP_ERROR_CHECK(iperf_cmd_register_iperf());
    const esp_console_cmd_t txpower_cmd = {
        .command = "txpower",
        .help = "Get or set Wi-Fi maximum TX power (2-6 dBm requests)",
        .hint = "[2|3|4|5|6]",
        .func = txpower_command,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&txpower_cmd));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));

    wifi_config_t wifi_config = { .ap = {
        .ssid = WIFI_SSID,
        .ssid_len = sizeof(WIFI_SSID) - 1,
        .password = WIFI_PASSWORD,
        .channel = WIFI_CHANNEL,
        .max_connection = 1,
        .authmode = WIFI_AUTH_OPEN,
        .pmf_cfg = { .required = false },
    }};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    wifi_country_t country = { .cc = "FI", .schan = 1, .nchan = 13,
                               .max_tx_power = 7, .policy = WIFI_COUNTRY_POLICY_MANUAL };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(WIFI_TX_POWER_QDBM));
    ESP_LOGI(TAG, "AP started; connect the TX station to '%s'", WIFI_SSID);

    init_console();
}
