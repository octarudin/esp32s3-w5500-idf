#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "driver/gpio.h"


#include "esp_eth.h"
#include "esp_eth_driver.h"
#include "esp_eth_mac_w5500.h"
#include "esp_eth_phy_w5500.h"


#define ETH_SPI_HOST SPI3_HOST

#define PIN_CS    16
#define PIN_MOSI  5
#define PIN_SCLK  6
#define PIN_MISO  7
#define PIN_RST   14
#define PIN_INT   -1


const char *ETH_TAG = "eth";
uint8_t eth_mac[6] = { 0x02, 0x00, 0x00, 0x12, 0x34, 0x56 };

static spi_device_interface_config_t devcfg = {
    .mode = 0,
    .clock_speed_hz = 8 * 1000 * 1000, // 20 MHz
    .spics_io_num = PIN_CS,
    .queue_size = 20,
};

static void spi_bus_init_w5500(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_MISO,
        .mosi_io_num = PIN_MOSI,
        .sclk_io_num = PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096
    };

    ESP_ERROR_CHECK(spi_bus_initialize(ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
}

bool eth_w5500_init(void)
{
    eth_w5500_config_t w5500_cfg = ETH_W5500_DEFAULT_CONFIG(ETH_SPI_HOST, &devcfg);
    w5500_cfg.int_gpio_num = PIN_INT;
    w5500_cfg.poll_period_ms = 100;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_cfg, &mac_cfg);

    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num = PIN_RST;
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;

    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth_handle));
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, eth_mac));

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_netif_set_default_netif(eth_netif));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    return true;
}

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(ETH_TAG, "Ethernet Link Up");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(ETH_TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(ETH_TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(ETH_TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    ESP_LOGI(ETH_TAG, "Got IP Address");
    ESP_LOGI(ETH_TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(ETH_TAG, "MASK: " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(ETH_TAG, "GW: " IPSTR, IP2STR(&event->ip_info.gw));
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(
        ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    spi_bus_init_w5500();
    eth_w5500_init();

}
