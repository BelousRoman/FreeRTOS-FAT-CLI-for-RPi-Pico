
/* hw_config.c
Copyright 2021 Carl John Kugler III

Licensed under the Apache License, Version 2.0 (the License); you may not use
this file except in compliance with the License. You may obtain a copy of the
License at

   http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed
under the License is distributed on an AS IS BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
*/

/*
This file should be tailored to match the hardware design.

See 
  https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico/tree/main#customizing-for-the-hardware-configuration

*/

/* Hardware configuration for Expansion Module Type A
See https://oshwlab.com/carlk3/rpi-pico-sd-card-expansion-module-1
*/

#pragma GCC diagnostic ignored "-Wunused-variable"

#include "hw_config.h"

// Hardware Configuration of SPI "object"
static spi_t spi  = {  // One for each RP2040 SPI component used
    .hw_inst = spi0,  // SPI component
    .sck_gpio = 2,  // GPIO number (not Pico pin number)
    .mosi_gpio = 3,
    .miso_gpio = 4,
    .set_drive_strength = true,
    .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA,
    .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
    .no_miso_gpio_pull_up = true,
    .DMA_IRQ_num = DMA_IRQ_0,
    .use_exclusive_DMA_IRQ_handler = true,
    // .baud_rate = 125 * 1000 * 1000 / 4,  // 31250000 Hz 
    .baud_rate = 125 * 1000 * 1000 / 8
};

/* SPI Interface */
static sd_spi_if_t spi_if = {
        .spi = &spi,  // Pointer to the SPI driving this card
        .ss_gpio = 7,     // The SPI slave select GPIO for this SD card
        .set_drive_strength = true,
        .ss_gpio_drive_strength = GPIO_DRIVE_STRENGTH_4MA
};

/* SDIO Interface */
static sd_sdio_if_t sdio_if = {
    /*
    Pins CLK_gpio, D1_gpio, D2_gpio, and D3_gpio are at offsets from pin D0_gpio.
    The offsets are determined by sd_driver\SDIO\rp2040_sdio.pio.
        CLK_gpio = (D0_gpio + SDIO_CLK_PIN_D0_OFFSET) % 32;
        As of this writing, SDIO_CLK_PIN_D0_OFFSET is 30,
            which is -2 in mod32 arithmetic, so:
        CLK_gpio = D0_gpio -2.
        D1_gpio = D0_gpio + 1;
        D2_gpio = D0_gpio + 2;
        D3_gpio = D0_gpio + 3;
    */
    .CMD_gpio = 3,
    .D0_gpio = 4,
    .CLK_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .CMD_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .D0_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .D1_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .D2_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .D3_gpio_drive_strength = GPIO_DRIVE_STRENGTH_12MA,
    .SDIO_PIO = pio1,
    .DMA_IRQ_num = DMA_IRQ_1,
    .baud_rate = 125 * 1000 * 1000 / 4  // 31250000 Hz
    // .baud_rate = 125 * 1000 * 1000 / 8
};

// Hardware Configuration of the SD Card socket "object"
#if USE_SPI_INTERFACE
static sd_card_t sd_card = {
    .device_name = "sd0",
    .mount_point = "/sd0",
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,  // Pointer to the SPI interface driving this card

    // SD Card detect:
    .use_card_detect = true,
    .card_detect_gpio = 9,  
    .card_detected_true = 0, // What the GPIO read returns when a card is present.
};
#else
static sd_card_t sd_card = {    
    .device_name = "sd0",
    .mount_point = "/sd0",
    .type = SD_IF_SDIO,
    .sdio_if_p = &sdio_if,
    // SD Card detect:
    .use_card_detect = true,
    .card_detect_gpio = 9,  
    .card_detected_true = 0  // What the GPIO read returns when a card is present.
};
#endif

/* ********************************************************************** */

size_t sd_get_num() { return 1; }

sd_card_t *sd_get_by_num(size_t num) {
    if (0 == num)
        return &sd_card;
    else
        return NULL;
}

/* [] END OF FILE */
