idf_component_register(
    SRCS 
        "main.c"
        "input.c"
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
        esp_wifi

        hagl
        hagl_hal

        led_strip
        ebtn
)

set(WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../www")

if(EXISTS ${WEB_DIR})
    spiffs_create_partition_image(www ${WEB_DIR} FLASH_IN_PROJECT)
else()
    message(FATAL_ERROR "Web directory '${WEB_DIR}' doesn't exist.")
endif()