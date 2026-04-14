/*
 * basic_connect — esp-wolfssh-client example
 *
 * Connects to WiFi, opens an SSH session (password auth), streams all output
 * to the serial monitor, and forwards serial input back to the shell.
 *
 * Configure host/port/user/pass/WiFi via: idf.py menuconfig → "Basic Connect Example"
 */

#include "esp_wolfssh_client.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "basic_connect";

// ---------------------------------------------------------------------------
// WiFi helpers
// ---------------------------------------------------------------------------

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_eg;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected");
        xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_connect(void)
{
    s_wifi_eg = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_wifi, inst_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &inst_wifi));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &inst_ip));

    wifi_config_t wcfg = {
        .sta = {
            .ssid     = CONFIG_EXAMPLE_WIFI_SSID,
            .password = CONFIG_EXAMPLE_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// ---------------------------------------------------------------------------
// SSH callbacks
// ---------------------------------------------------------------------------

static volatile bool s_ssh_active = false;

static void on_data(const uint8_t *data, size_t len, void *ctx)
{
    // Write raw bytes directly to UART0 (the serial monitor)
    uart_write_bytes(UART_NUM_0, (const char *)data, len);
}

static void on_connected(void *ctx)
{
    ESP_LOGI(TAG, "shell open — type into serial monitor");
    s_ssh_active = true;
}

static void on_disconnected(int reason, void *ctx)
{
    ESP_LOGI(TAG, "disconnected (reason=%d)", reason);
    s_ssh_active = false;
}

// ---------------------------------------------------------------------------
// UART reader task — forwards serial monitor keystrokes to the SSH session
// ---------------------------------------------------------------------------

static void uart_reader_task(void *arg)
{
    uint8_t buf[64];
    while (1) {
        int n = uart_read_bytes(UART_NUM_0, buf, sizeof(buf), pdMS_TO_TICKS(100));
        if (n > 0 && s_ssh_active)
            ssh_client_send(buf, (size_t)n);
    }
}

// ---------------------------------------------------------------------------
// app_main
// ---------------------------------------------------------------------------

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "connecting to WiFi...");
    if (!wifi_connect()) {
        ESP_LOGE(TAG, "WiFi connect failed");
        return;
    }

    wolfSSH_Init();

    // Configure UART0 for raw byte reads (no line buffering)
    uart_config_t ucfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_0, &ucfg);
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    xTaskCreate(uart_reader_task, "uart_reader", 2048, NULL, 4, NULL);

    ssh_client_config_t cfg = {
        .host     = CONFIG_EXAMPLE_SSH_HOST,
        .port     = CONFIG_EXAMPLE_SSH_PORT,
        .user     = CONFIG_EXAMPLE_SSH_USER,
        .password = CONFIG_EXAMPLE_SSH_PASS[0] ? CONFIG_EXAMPLE_SSH_PASS : NULL,
        .term_cols = 80,
        .term_rows = 24,
        .callbacks = {
            .on_data         = on_data,
            .on_connected    = on_connected,
            .on_disconnected = on_disconnected,
        },
    };

    ESP_LOGI(TAG, "connecting to %s:%d as %s",
             cfg.host, cfg.port, cfg.user);
    ESP_ERROR_CHECK(ssh_client_connect(&cfg));

    // Wait for session to end, then loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (!s_ssh_active && !ssh_client_is_connected()) {
            ESP_LOGI(TAG, "reconnecting in 5s...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            s_ssh_active = false;
            ssh_client_connect(&cfg);
        }
    }
}
