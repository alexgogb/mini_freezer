idf_component_register(
    SRCS "shift_register_595_driver.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio
)