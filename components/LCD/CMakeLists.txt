idf_component_register(
    SRCS "LCD_1602_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES shift_register_595_driver
)