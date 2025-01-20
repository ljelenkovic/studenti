#ifndef LL_HELPER_H
#define LL_HELPER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hd44780.h"

// Defines pins for buttons
#define BUTTON_NAVIGATE 13
#define BUTTON_CONFIRM  32

// Defines pins for LCD
#define LCD_RS 33
#define LCD_E 14
#define LCD_D4 25
#define LCD_D5 26
#define LCD_D6 27
#define LCD_D7 23
#define LCD_BL HD44780_NOT_USED

// Defines for LEDs
#define LED_GREEN 4
#define LED_RED_B1 21
#define LED_BLUE_B1 18
#define LED_RED_B2 19
#define LED_BLUE_B2 22


extern hd44780_t lcd;


#define MAX_ACTIVE_TASKS 20


void lcd_init(void);
void led_init(void);
bool add_active_task(TaskHandle_t task);
void kill_child_tasks(void);
#endif // LL_HELPER_H 