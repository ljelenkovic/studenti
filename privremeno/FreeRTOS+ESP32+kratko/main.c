#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#define LED_PIN_1 GPIO_NUM_18
#define LED_PIN_2 GPIO_NUM_23
#define BUTTON_PIN GPIO_NUM_26

SemaphoreHandle_t pauseSemaphore;
QueueHandle_t xTestQueue;

void blink(void *pvParameter)
{
    uint32_t ledPin = (uint32_t)pvParameter;
    gpio_set_direction(ledPin, GPIO_MODE_OUTPUT);

    int pressed;

    while(1)
    {
        vTaskDelay(5000/portTICK_PERIOD_MS);
        xSemaphoreTake(pauseSemaphore, portMAX_DELAY);
        gpio_set_level(LED_PIN_2, 0);
        while (xQueueReceive(xTestQueue, &pressed, 100/portTICK_PERIOD_MS) == pdTRUE) {
            gpio_set_level(ledPin, 1);
            vTaskDelay(100/portTICK_PERIOD_MS);
            gpio_set_level(ledPin, 0);
            vTaskDelay(1000/portTICK_PERIOD_MS);
        }
        xSemaphoreGive(pauseSemaphore);
    }
}

void Button_Task(void *param) {
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_PIN, GPIO_PULLUP_ONLY);

    int lastState = gpio_get_level(BUTTON_PIN);

    while (1) {
        if (xSemaphoreTake(pauseSemaphore, portMAX_DELAY) == pdTRUE) {
            gpio_set_level(LED_PIN_2, 1);
            int currentState = gpio_get_level(BUTTON_PIN);
            // Detect button press (falling edge)
            if (lastState == 1 && currentState == 0) {
                int pressed = 1;
                xQueueSend(xTestQueue, &pressed, portMAX_DELAY);
            }

            lastState = currentState;
            xSemaphoreGive(pauseSemaphore);
            vTaskDelay(100/portTICK_PERIOD_MS); // Delay to not immediately try to retake the semaphore
        }
    }
}

void app_main(void)
{
    gpio_set_direction(LED_PIN_2, GPIO_MODE_OUTPUT);
    pauseSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(pauseSemaphore);
    xTestQueue = xQueueCreate(100, sizeof(int));
    xTaskCreate(&blink, "Blinker", 2048, (void *)LED_PIN_1, 1, NULL);
    xTaskCreate(&Button_Task, "Button Task", 2048, NULL, 2, NULL);
}