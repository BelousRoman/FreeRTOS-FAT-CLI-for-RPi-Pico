#include "lwip/apps/httpd.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwipopts.h"
//
#include "FreeRTOS.h"
#include "task.h"
//
#include "sd_filesystem.h"

void MainTask(__unused void *params) {
    // Initialize filesytem and mount SD card
    sd_init_mount();    

    cyw43_arch_init();

    cyw43_arch_enable_sta_mode();

    // Connect to the WiFI network - loop until connected
    while(cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000) != 0){
        printf("Attempting to connect to %s...\n", WIFI_SSID);
    }
    // Print a success message once connected
    printf("Connected! \n");
    
    // Initialise web server
    cyw43_arch_lwip_begin();
    httpd_init();
    cyw43_arch_lwip_end();
    printf("Http server initialised\n");

    printf("\nListening at %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)));
    
    vTaskDelete(NULL);
}

int main() {
    stdio_init_all();
    xTaskCreate(MainTask, "TestMainThread", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    vTaskStartScheduler();
}