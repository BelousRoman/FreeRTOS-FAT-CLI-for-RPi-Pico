if (NOT DEFINED FREERTOS_FAT_LIB)
        set(FREERTOS_FAT_LIB FreeRTOS+FAT+CLI)
endif()

if (NOT DEFINED FREERTOS_FAT_PATH)
        set(FREERTOS_FAT_PATH "../Lab-Project-FreeRTOS-FAT")
endif()

add_library(${FREERTOS_FAT_LIB} INTERFACE)
target_sources(${FREERTOS_FAT_LIB} INTERFACE
        ${FREERTOS_FAT_PATH}/ff_crc.c
        ${FREERTOS_FAT_PATH}/ff_dir.c
        ${FREERTOS_FAT_PATH}/ff_error.c
        ${FREERTOS_FAT_PATH}/ff_fat.c
        ${FREERTOS_FAT_PATH}/ff_file.c
        ${FREERTOS_FAT_PATH}/ff_format.c
        ${FREERTOS_FAT_PATH}/ff_ioman.c
        ${FREERTOS_FAT_PATH}/ff_locking.c
        ${FREERTOS_FAT_PATH}/ff_memory.c
        ${FREERTOS_FAT_PATH}/ff_stdio.c
        ${FREERTOS_FAT_PATH}/ff_string.c
        ${FREERTOS_FAT_PATH}/ff_sys.c
        ${FREERTOS_FAT_PATH}/ff_time.c 
        portable/RP2040/dma_interrupts.c
        portable/RP2040/ff_sddisk.c
        portable/RP2040/sd_card.c
        portable/RP2040/SPI/sd_card_spi.c
        portable/RP2040/SPI/sd_spi.c
        portable/RP2040/SPI/my_spi.c
        portable/RP2040/SDIO/sd_card_sdio.c
        portable/RP2040/SDIO/rp2040_sdio.c
        src/crash.c
        src/crc.c
        src/ff_utils.c
        src/file_stream.c
        src/freertos_callbacks.c
        src/FreeRTOS_strerror.c
        src/FreeRTOS_time.c
        src/my_debug.c
        src/sd_timeouts.c
        src/util.c
)
target_link_libraries(${FREERTOS_FAT_LIB} INTERFACE
        cmsis_core
        #FreeRTOS-Kernel
        #FreeRTOS-Kernel-Heap4
        hardware_adc
        hardware_clocks
        hardware_dma
        hardware_pio
        #hardware_rtc
        hardware_spi
        hardware_timer
        pico_aon_timer
        pico_multicore
        pico_stdlib
        pico_sync
)
target_include_directories(${FREERTOS_FAT_LIB} INTERFACE 
        include/ 
        ${FREERTOS_FAT_PATH}/include/
        portable/RP2040/
)
