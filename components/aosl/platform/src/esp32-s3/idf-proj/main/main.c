/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"

#include <pthread.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "api/aosl_test.h"

#define CONFIG_WIFI_SSID "NXIOT"
#define CONFIG_WIFI_PASSWORD "88888888"
#define CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL 3

static int g_flag_got_ip = 0;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  printf("wifi event %ld\n", event_id);
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    printf("wifi sta mode connect.\n");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    printf("got ip: " IPSTR "\n", IP2STR(&event->ip_info.ip));
    g_flag_got_ip = 1;
  }
}

/*init wifi as sta */
static void setup_wifi(void)
{
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
  assert(sta_netif);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

  wifi_config_t wifi_config = {
    .sta = {
      .ssid            = CONFIG_WIFI_SSID,
      .password        = CONFIG_WIFI_PASSWORD,
      .listen_interval = CONFIG_EXAMPLE_WIFI_LISTEN_INTERVAL,
    },
  };
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  esp_wifi_set_ps(WIFI_PS_NONE);

  // Wait for IP address, max 10 seconds
  int wait_count = 0;
  const int max_wait_ms = 10000; // 10 seconds
  const int check_interval_ms = 100; // Check every 100ms
  const int max_iterations = max_wait_ms / check_interval_ms;
  
  printf("Waiting for IP address (max %d seconds)...\n", max_wait_ms / 1000);
  
  while (g_flag_got_ip == 0 && wait_count < max_iterations) {
    vTaskDelay(check_interval_ms / portTICK_PERIOD_MS);
    wait_count++;
    if (wait_count % 10 == 0) {
      printf("Still waiting for IP... (%d/%d seconds)\n", wait_count / 10, max_wait_ms / 1000);
    }
  }
  
  if (g_flag_got_ip == 1) {
    printf("Successfully got IP address!\n");
  } else {
    printf("Timeout: Failed to get IP address after %d seconds\n", max_wait_ms / 1000);
  }
}

void app_main(void)
{
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // init and start wifi
  setup_wifi();

  // do aosl test
  if (g_flag_got_ip) {
    aosl_test();
  }

  while (1) {
    printf("nop loop...\n");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
