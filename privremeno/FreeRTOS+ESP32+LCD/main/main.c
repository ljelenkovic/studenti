
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "LL_Helper.h"

volatile int menu_index = 0;
volatile bool is_running = false;

#define MENU_ITEMS 4


volatile bool navigate_requested = false;
volatile bool confirm_requested = false;

const char *menu_options[] = {
    "1. Barbe",
    "rs",
    "2. Philo",
    "sophers"};

TaskHandle_t active_task_M = NULL;

extern void barber_run(void *arg);
extern void philosophers_run(void *arg);

bool interrupt_active = false;



void IRAM_ATTR interrupt_handler(void *arg)
{
    
     uint32_t gpio_num = (uint32_t) arg;
    if (gpio_num == BUTTON_NAVIGATE) {
        navigate_requested = true;
    } else if (gpio_num == BUTTON_CONFIRM) {
        confirm_requested = true;
    }
}

void display_menu(void)
{

    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, menu_options[menu_index]);
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, menu_options[menu_index + 1]);
}

void nav_menu(void)
{

    vTaskDelay(pdMS_TO_TICKS(300)); // Debounce

    if (!is_running)
    {
        display_menu();
        printf("Menu: %s%s\n", menu_options[menu_index], menu_options[menu_index + 1]);
        menu_index = (menu_index + 2) % MENU_ITEMS;
    }
}

void conf_menu(void)
{
    vTaskDelay(pdMS_TO_TICKS(300)); // Debounce
    if (!is_running)
    {
        is_running = true;
        if (menu_index == 2)
        {
            printf("\nLaunching Sleeping Barber problem.\n");
            xTaskCreate(barber_run, "BarberShop", 4096, NULL, 5, &active_task_M);
            add_active_task(active_task_M);
        }
        else if (menu_index == 0)
        {
            printf("\nLaunching Dining Philosophers problem.\n");
            xTaskCreate(philosophers_run, "Philosophers", 4096, NULL, 5, &active_task_M);
            add_active_task(active_task_M);
        }

    }
}

void button_init(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_NAVIGATE) | (1ULL << BUTTON_CONFIRM),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE};

    gpio_config(&io_conf);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BUTTON_NAVIGATE, interrupt_handler, (void *)BUTTON_NAVIGATE);
    gpio_isr_handler_add(BUTTON_CONFIRM, interrupt_handler, (void *)BUTTON_CONFIRM);
}

void handle_interrupt(void)
{

    vTaskDelay(pdMS_TO_TICKS(300)); // Debounce
   printf("\nInterrupt triggered: Returning to main menu...\n");
    kill_child_tasks();
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "INTERRUPT");
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "T");

    vTaskDelay(pdMS_TO_TICKS(2000));

    gpio_set_level(LED_GREEN, 0);
    gpio_set_level(LED_RED_B1, 0);
    gpio_set_level(LED_BLUE_B1, 0);
    gpio_set_level(LED_RED_B2, 0);
    gpio_set_level(LED_BLUE_B2, 0);


    menu_index = 0;
    nav_menu();
}


void app_main(void)
{
    lcd_init();
    led_init();
    button_init();

    printf("Main menu: Use Left button to navigate and Right button to confirm\n");

    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "Select: ");

    nav_menu();

    while (1) {
        /// Scroling through the menu
        if (navigate_requested && !is_running) {
            navigate_requested = false;
            nav_menu();  
        }

        /// Confirming the selection
        if (confirm_requested) {
            confirm_requested = false;
            if (!is_running) {
                //If the program is not running, confirm the selection
                conf_menu();
            } else {
                //If the program is running, interrupt it and return to main menu
                //Interrupt routine
                if (active_task_M != NULL) {
                    handle_interrupt();
                    active_task_M = NULL;
                }
                is_running = false;
                // Return to main menu
                menu_index = 0;
                nav_menu();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
