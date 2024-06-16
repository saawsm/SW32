idf_component_register(
    SRCS 
        "main.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        log
        driver
        esp_adc
        
        spiffs
        fatfs
        sdmmc
        
        nvs_flash
        mdns
        esp_http_server
        esp_netif
        protocol_examples_common

        hagl
        hagl_hal

        led_strip
        ebtn
)

set(WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../www")
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../data")

if(EXISTS ${WEB_DIR})
    spiffs_create_partition_image(www ${WEB_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "Web directory '${WEB_DIR}' doesn't exist.")
endif()

if(EXISTS ${DATA_DIR})
    spiffs_create_partition_image(data ${DATA_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "Data directory '${DATA_DIR}' doesn't exist.")
endif()