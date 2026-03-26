idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES shift_register_595_driver LCD_1602_driver driver
)
