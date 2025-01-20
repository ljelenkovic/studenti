#include "LL_Helper.h"

TaskHandle_t active_tasks_x[MAX_ACTIVE_TASKS] = {NULL};


hd44780_t lcd = {
    .pins = {
        .rs = LCD_RS,
        .e = LCD_E,
        .d4 = LCD_D4,
        .d5 = LCD_D5,
        .d6 = LCD_D6,
        .d7 = LCD_D7,
        .bl = LCD_BL},
    .font = HD44780_FONT_5X8,
    .lines = 2,
    .backlight = false};

void lcd_init(void){
    hd44780_init(&lcd);
    hd44780_clear(&lcd);
    hd44780_control(&lcd, true, false, false);
}


void led_init(void){
    gpio_reset_pin(LED_GREEN);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_RED_B1);
    gpio_set_direction(LED_RED_B1, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_BLUE_B1);
    gpio_set_direction(LED_BLUE_B1, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_RED_B2);
    gpio_set_direction(LED_RED_B2, GPIO_MODE_OUTPUT);

    gpio_reset_pin(LED_BLUE_B2);
    gpio_set_direction(LED_BLUE_B2, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_RED_B1, 0);
    gpio_set_level(LED_BLUE_B1, 0);
    gpio_set_level(LED_RED_B2, 0);
    gpio_set_level(LED_BLUE_B2, 0);

    gpio_set_level(LED_GREEN, 1);
    gpio_set_level(LED_RED_B1, 1);
    gpio_set_level(LED_BLUE_B1, 1);
    gpio_set_level(LED_RED_B2, 1);
    gpio_set_level(LED_BLUE_B2, 1);
    
    vTaskDelay(pdMS_TO_TICKS(2000));

    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_RED_B1, 0);
    gpio_set_level(LED_BLUE_B1, 0);
    gpio_set_level(LED_RED_B2, 0);
    gpio_set_level(LED_BLUE_B2, 0);
}


bool add_active_task(TaskHandle_t task)
{
    for (int i = 0; i < MAX_ACTIVE_TASKS; i++)
    {
        if (active_tasks_x[i] == NULL)
        {
            active_tasks_x[i] = task;
            return true;
        }
    }
    return false;
}

void kill_child_tasks(void)
{
    for (int i = 0; i < MAX_ACTIVE_TASKS; i++)
    {
        if (active_tasks_x[i] != NULL)
        {
            vTaskDelete(active_tasks_x[i]);
            active_tasks_x[i] = NULL;
        }
    }
}