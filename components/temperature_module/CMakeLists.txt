idf_component_register(
    SRCS "temperature_module.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio
)