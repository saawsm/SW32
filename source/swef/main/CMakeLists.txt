idf_component_register(
    SRCS 
        "main.cpp"
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

        hagl
        hagl_hal

        led_strip
        ebtn
)
