/* sd_card.c
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

/* Standard includes. */
#include <inttypes.h>
#include <string.h>
//
#include "pico/mutex.h"
//
#include "SDIO/SdioCard.h"
#include "SPI/sd_card_spi.h"
#include "hw_config.h"  // Hardware Configuration of the SPI and SD Card "objects"
#include "my_debug.h"
#include "sd_card_constants.h"
//
#include "sd_card.h"
//
// #include "ff.h" /* Obtains integer types */
//
// #include "diskio.h" /* Declarations of disk functions */  // Needed for STA_NOINIT, ...

#define TRACE_PRINTF(fmt, args...)
// #define TRACE_PRINTF printf

// An SD card can only do one thing at a time.
void sd_lock(sd_card_t *sd_card_p) {
    myASSERT(sd_card_p->mutex);
    xSemaphoreTake(sd_card_p->mutex, portMAX_DELAY);
    myASSERT(0 == sd_card_p->owner);
    sd_card_p->owner = xTaskGetCurrentTaskHandle();
}
void sd_unlock(sd_card_t *sd_card_p) {
    myASSERT(sd_card_p->mutex);
    myASSERT(xTaskGetCurrentTaskHandle() == sd_card_p->owner);
    sd_card_p->owner = 0;
    xSemaphoreGive(sd_card_p->mutex);
}
bool sd_is_locked(sd_card_t *sd_card_p) {
    myASSERT(sd_card_p->mutex);
    // Returns:
    // pdTRUE if the semaphore was obtained. pdFALSE if xTicksToWait expired without the semaphore becoming available.}
    // BaseType_t rc = xSemaphoreTake(sd_card_p->mutex, 0);
    // return (pdFALSE == rc);
    return xTaskGetCurrentTaskHandle() == xSemaphoreGetMutexHolder(sd_card_p->mutex);
}

/* Return non-zero if the SD-card is present. */
bool sd_card_detect(sd_card_t *sd_card_p) {
    TRACE_PRINTF("> %s\r\n", __FUNCTION__);
    if (!sd_card_p->use_card_detect) {
        sd_card_p->m_Status &= ~STA_NODISK;
        return true;
    }
    /*!< Check GPIO to detect SD */
    if (gpio_get(sd_card_p->card_detect_gpio) == sd_card_p->card_detected_true) {
        // The socket is now occupied
        sd_card_p->m_Status &= ~STA_NODISK;
        TRACE_PRINTF("SD card detected!\r\n");
        return true;
    } else {
        // The socket is now empty
        sd_card_p->m_Status |= (STA_NODISK | STA_NOINIT);
        sd_card_p->card_type = SDCARD_NONE;
        EMSG_PRINTF("No SD card detected!\r\n");
        return false;
    }
}

bool sd_init_driver() {
    static bool initialized;
    auto_init_mutex(initialized_mutex);
    mutex_enter_blocking(&initialized_mutex);
    if (!initialized) {
        for (size_t i = 0; i < sd_get_num(); ++i) {
            sd_card_t *sd_card_p = sd_get_by_num(i);
            myASSERT(sd_card_p->device_name);
            myASSERT(sd_card_p->mount_point);

            switch (sd_card_p->type) {
                case SD_IF_SPI:
                    sd_spi_ctor(sd_card_p);
                    if (!my_spi_init(sd_card_p->spi_if_p->spi)) {
                        mutex_exit(&initialized_mutex);
                        return false;
                    }
                    break;
                case SD_IF_SDIO:
                    sd_sdio_ctor(sd_card_p);
                    break;
            }  // switch (sd_card_p->type)

            if (sd_card_p->use_card_detect) {
                if (sd_card_p->card_detect_use_pull) {
                    if (sd_card_p->card_detect_pull_hi) {
                        gpio_pull_up(sd_card_p->card_detect_gpio);
                    } else {
                        gpio_pull_down(sd_card_p->card_detect_gpio);
                    }
                }
                gpio_init(sd_card_p->card_detect_gpio);
            }
        }  // for

        initialized = true;
    }
    mutex_exit(&initialized_mutex);
    return true;
}

void cidDmp(sd_card_t *sd_card_p, printer_t printer) {
    (*printer)("\nManufacturer ID: ");
    (*printer)("0x%x\n", sd_card_p->cid.mid);
    (*printer)("OEM ID: ");
    (*printer)("%c%c\n", sd_card_p->cid.oid[0], sd_card_p->cid.oid[1]);
    (*printer)("Product: ");
    for (uint8_t i = 0; i < 5; i++) {
        (*printer)("%c", sd_card_p->cid.pnm[i]);
    }
    (*printer)("\nRevision: ");
    (*printer)("%d.%d\n", CID_prvN(&sd_card_p->cid), CID_prvM(&sd_card_p->cid));
    (*printer)("Serial number: ");
    (*printer)("0x%lx\n", CID_psn(&sd_card_p->cid));
    (*printer)("Manufacturing date: ");
    (*printer)("%d/%d\n", CID_mdtMonth(&sd_card_p->cid), CID_mdtYear(&sd_card_p->cid));
    (*printer)("\n");
}
void csdDmp(sd_card_t *sd_card_p, printer_t printer) {
    uint32_t c_size, c_size_mult, read_bl_len;
    uint32_t block_len, mult, blocknr;
    uint32_t hc_c_size;
    uint64_t blocks = 0, capacity = 0;
    bool erase_single_block_enable = 0;
    uint8_t erase_sector_size = 0;

    // csd_structure : csd[127:126]
    int csd_structure = ext_bits16(sd_card_p->csd.csd, 127, 126);
    switch (csd_structure) {
        case 0:
            c_size = ext_bits16(sd_card_p->csd.csd, 73, 62);       // c_size        : csd[73:62]
            c_size_mult = ext_bits16(sd_card_p->csd.csd, 49, 47);  // c_size_mult   : csd[49:47]
            read_bl_len =
                ext_bits16(sd_card_p->csd.csd, 83, 80);  // read_bl_len   : csd[83:80] - the
                                                       // *maximum* read block length
            block_len = 1 << read_bl_len;              // BLOCK_LEN = 2^READ_BL_LEN
            mult = 1 << (c_size_mult +
                         2);                // MULT = 2^C_SIZE_MULT+2 (C_SIZE_MULT < 8)
            blocknr = (c_size + 1) * mult;  // BLOCKNR = (C_SIZE+1) * MULT
            capacity = (uint64_t)blocknr *
                       block_len;  // memory capacity = BLOCKNR * BLOCK_LEN
            blocks = capacity / _block_size;

            (*printer)("Standard Capacity: c_size: %" PRIu32 "\r\n", c_size);
            (*printer)("Sectors: 0x%llx : %llu\r\n", blocks, blocks);
            (*printer)("Capacity: 0x%llx : %llu MiB\r\n", capacity,
                       (capacity / (1024U * 1024U)));
            break;

        case 1:
            hc_c_size =
                ext_bits16(sd_card_p->csd.csd, 69, 48);  // device size : C_SIZE : [69:48]
            blocks = (hc_c_size + 1) << 10;            // block count = C_SIZE+1) * 1K
                                                       // byte (512B is block size)

            /* ERASE_BLK_EN
            The ERASE_BLK_EN defines the granularity of the unit size of the data to be erased. The erase
            operation can erase either one or multiple units of 512 bytes or one or multiple units (or sectors) of
            SECTOR_SIZE. If ERASE_BLK_EN=0, the host can erase one or multiple units of SECTOR_SIZE.
            If ERASE_BLK_EN=1 the host can erase one or multiple units of 512 bytes.
            */
            erase_single_block_enable = ext_bits16(sd_card_p->csd.csd, 46, 46);

            /* SECTOR_SIZE
            The size of an erasable sector. The content of this register is a 7-bit binary coded value, defining the
            number of write blocks. The actual size is computed by increasing this number
            by one. A value of zero means one write block, 127 means 128 write blocks.
            */
            erase_sector_size = ext_bits16(sd_card_p->csd.csd, 45, 39) + 1;

            (*printer)("SDHC/SDXC Card: hc_c_size: %" PRIu32 "\r\n", hc_c_size);
            (*printer)("Sectors: %llu\r\n", blocks);
            (*printer)("Capacity: %llu MiB (%llu MB)\r\n", blocks / 2048, blocks * _block_size / 1000000);
            (*printer)("ERASE_BLK_EN: %s\r\n", erase_single_block_enable ? "units of 512 bytes" : "units of SECTOR_SIZE");
            (*printer)("SECTOR_SIZE (size of an erasable sector): %d (%lu bytes)\r\n",
                       erase_sector_size, (uint32_t)(erase_sector_size ? 512 : 1) * erase_sector_size);
            break;

        default:
            (*printer)("CSD struct unsupported\r\n");
    };
}

#define KB 1024
#define MB (1024 * 1024)

/* AU (Allocation Unit):
is a physical boundary of the card and consists of one or more blocks and its
size depends on each card. */
bool sd_allocation_unit(sd_card_t *sd_card_p, size_t *au_size_bytes_p) {
    if (SD_IF_SPI == sd_card_p->type)
        return false;  // SPI can't do full SD Status

    uint8_t status[64] = {0};
    bool ok = rp2040_sdio_get_sd_status(sd_card_p, status);
    if (!ok)
        return false;
    // 431:428 AU_SIZE
    uint8_t au_size = ext_bits(64, status, 431, 428);
    switch (au_size) {
        // AU_SIZE Value Definition
        case 0x0:
            *au_size_bytes_p = 0;
            break;  // Not Defined
        case 0x1:
            *au_size_bytes_p = 16 * KB;
            break;
        case 0x2:
            *au_size_bytes_p = 32 * KB;
            break;
        case 0x3:
            *au_size_bytes_p = 64 * KB;
            break;
        case 0x4:
            *au_size_bytes_p = 128 * KB;
            break;
        case 0x5:
            *au_size_bytes_p = 256 * KB;
            break;
        case 0x6:
            *au_size_bytes_p = 512 * KB;
            break;
        case 0x7:
            *au_size_bytes_p = 1 * MB;
            break;
        case 0x8:
            *au_size_bytes_p = 2 * MB;
            break;
        case 0x9:
            *au_size_bytes_p = 4 * MB;
            break;
        case 0xA:
            *au_size_bytes_p = 8 * MB;
            break;
        case 0xB:
            *au_size_bytes_p = 12 * MB;
            break;
        case 0xC:
            *au_size_bytes_p = 16 * MB;
            break;
        case 0xD:
            *au_size_bytes_p = 24 * MB;
            break;
        case 0xE:
            *au_size_bytes_p = 32 * MB;
            break;
        case 0xF:
            *au_size_bytes_p = 64 * MB;
            break;
        default:
            configASSERT(false);
    }
    return true;
}

sd_card_t *sd_get_by_name(const char *const name) {
    configASSERT(name);
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->device_name, name)) return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}
sd_card_t *sd_get_by_mount_point(const char *const name) {
    configASSERT(name);
    for (size_t i = 0; i < sd_get_num(); ++i)
        if (0 == strcmp(sd_get_by_num(i)->mount_point, name)) return sd_get_by_num(i);
    DBG_PRINTF("%s: unknown name %s\n", __func__, name);
    return NULL;
}

/* [] END OF FILE */
