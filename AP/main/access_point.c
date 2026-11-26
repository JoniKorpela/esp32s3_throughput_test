#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_console.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "iperf_cmd.h"

#define WIFI_SSID "wifitesti"
#define WIFI_PASSWORD ""
#define WIFI_CHANNEL 6
#define WIFI_TX_POWER_QDBM 28      /* Quarter-dBm units: 28 = 7 dBm */
#define TEST_PEER_IP "192.168.4.2" /* First station DHCP lease on this one-station AP */

static const char *TAG = "access_point";
static uint32_t s_test_duration_seconds = 10;

static int txpower_command(int argc, char **argv)
{
    if (argc == 1)
    {
        int8_t power_qdbm;
        ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&power_qdbm));
        int applied_dbm = power_qdbm == 28 ? 7 : (power_qdbm == 20 ? 5 : 2);
        ESP_LOGI(TAG, "Maximum Wi-Fi TX power: %d dBm (driver value %d quarter-dBm)",
                 applied_dbm, power_qdbm);
        return 0;
    }
    if (argc != 2)
    {
        printf("Usage: txpower [2|5|7] (dBm)\n");
        return 1;
    }
    char *end = NULL;
    long dbm = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || (dbm != 2 && dbm != 5 && dbm != 7))
    {
        printf("Allowed values: 2, 5, or 7 dBm.\n");
        return 1;
    }
    int8_t requested_qdbm = (int8_t)(dbm * 4);
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(requested_qdbm));
    int8_t applied_qdbm;
    ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&applied_qdbm));
    ESP_LOGI(TAG, "Requested %ld dBm; driver applied %d dBm", dbm,
             applied_qdbm == 28 ? 7 : (applied_qdbm == 20 ? 5 : 2));
    return 0;
}

static int run_iperf_command(const char *command)
{
    int command_result = 0;
    esp_err_t err = esp_console_run(command, &command_result);
    if (err != ESP_OK || command_result != 0)
    {
        ESP_LOGE(TAG, "Could not start '%s' (err=%s, result=%d)",
                 command, esp_err_to_name(err), command_result);
        return 1;
    }
    return 0;
}

static int dur_command(int argc, char **argv)
{
    if (argc == 1) {
        printf("Test duration: %lu seconds (report interval: 5 seconds)\n",
               (unsigned long)s_test_duration_seconds);
        return 0;
    }
    if (argc != 2) {
        printf("Usage: dur <seconds> (1-3600)\n");
        return 1;
    }

    char *end = NULL;
    long seconds = strtol(argv[1], &end, 10);
    if (argv[1][0] == '\0' || *end != '\0' || seconds < 1 || seconds > 3600) {
        printf("Duration must be an integer from 1 to 3600 seconds.\n");
        return 1;
    }
    s_test_duration_seconds = (uint32_t)seconds;
    printf("Test duration set to %lu seconds (report interval: 5 seconds)\n",
           (unsigned long)s_test_duration_seconds);
    return 0;
}

static int server_command(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "tcp") != 0 && strcmp(argv[1], "udp") != 0))
    {
        printf("Usage: server tcp|udp\n");
        return 1;
    }

    char command[80];
    snprintf(command, sizeof(command), "iperf -s %s-t %lu -i 5",
             strcmp(argv[1], "udp") == 0 ? "-u " : "",
             (unsigned long)s_test_duration_seconds);
    printf("Starting iperf server: protocol=%s, listen=0.0.0.0:5001, duration=%lu s, interval=5 s\n",
           argv[1], (unsigned long)s_test_duration_seconds);
    return run_iperf_command(command);
}

static bool valid_bandwidth(const char *value)
{
    char *end = NULL;
    long amount = strtol(value, &end, 10);
    if (amount <= 0 || end == value || end[0] == '\0' || end[1] != '\0')
    {
        return false;
    }
    long multiplier;
    switch (end[0])
    {
    case 'k':
    case 'K':
        multiplier = 1000L;
        break;
    case 'm':
    case 'M':
        multiplier = 1000000L;
        break;
    case 'g':
    case 'G':
        multiplier = 1000000000L;
        break;
    default:
        return false;
    }
    return amount <= INT32_MAX / multiplier;
}

static int test_command(int argc, char **argv)
{
    char command[96];
    if (argc == 2 && strcmp(argv[1], "tcp") == 0)
    {
        snprintf(command, sizeof(command), "iperf -c %s -t %lu -i 5", TEST_PEER_IP,
                 (unsigned long)s_test_duration_seconds);
    }
    else if (argc == 3 && strcmp(argv[1], "udp") == 0 && valid_bandwidth(argv[2]))
    {
        snprintf(command, sizeof(command), "iperf -c %s -u -b %s -t %lu -i 5",
                 TEST_PEER_IP, argv[2], (unsigned long)s_test_duration_seconds);
    }
    else
    {
        printf("Usage: test tcp | test udp <rate, e.g. 5M>\n");
        return 1;
    }

    if (strcmp(argv[1], "udp") == 0) {
        printf("Starting iperf test: protocol=UDP, peer=%s:5001, target_rate=%s, duration=%lu s, interval=5 s\n",
               TEST_PEER_IP, argv[2], (unsigned long)s_test_duration_seconds);
    } else {
        printf("Starting iperf test: protocol=TCP, peer=%s:5001, duration=%lu s, interval=5 s (send as fast as possible)\n",
               TEST_PEER_IP, (unsigned long)s_test_duration_seconds);
    }
    return run_iperf_command(command);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START)
    {
        ESP_LOGI(TAG, "Open AP ready: SSID='%s', channel=%d, address=192.168.4.1", WIFI_SSID, WIFI_CHANNEL);
        ESP_LOGI(TAG, "Use 'server tcp' or 'server udp' for a field test");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        const wifi_event_ap_staconnected_t *event = event_data;
        ESP_LOGI(TAG, "Station joined, AID=%d", event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
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
        .help = "Get or set Wi-Fi maximum TX power (2, 5, or 7 dBm)",
        .hint = "[2|5|7]",
        .func = txpower_command,
    };
    const esp_console_cmd_t server_cmd = {
        .command = "server",
        .help = "Start one TCP or UDP iperf server run using the selected duration",
        .hint = "tcp|udp",
        .func = server_command,
    };
    const esp_console_cmd_t test_cmd = {
        .command = "test",
        .help = "Run a TCP or rate-limited UDP iperf test using the selected duration",
        .hint = "tcp | udp <rate>",
        .func = test_command,
    };
    const esp_console_cmd_t dur_cmd = {
        .command = "dur",
        .help = "Get or set test duration in seconds (report interval stays at 5 seconds)",
        .hint = "<seconds>",
        .func = dur_command,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&txpower_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&server_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&test_cmd));
    ESP_ERROR_CHECK(esp_console_cmd_register(&dur_cmd));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
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

    wifi_config_t wifi_config = {.ap = {
                                     .ssid = WIFI_SSID,
                                     .ssid_len = sizeof(WIFI_SSID) - 1,
                                     .password = WIFI_PASSWORD,
                                     .channel = WIFI_CHANNEL,
                                     .max_connection = 1,
                                     .authmode = WIFI_AUTH_OPEN,
                                     .pmf_cfg = {.required = false},
                                 }};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    wifi_country_t country = {.cc = "FI", .schan = 1, .nchan = 13, .max_tx_power = 7, .policy = WIFI_COUNTRY_POLICY_MANUAL};
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(WIFI_TX_POWER_QDBM));
    ESP_LOGI(TAG, "AP started; connect the TX station to '%s'", WIFI_SSID);

    init_console();
}
