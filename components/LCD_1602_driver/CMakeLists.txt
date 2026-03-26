idf_component_register(
    SRCS "LCD_1602_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES shift_register_595_driver
    PRIV_REQUIRES esp_driver_gpio
)