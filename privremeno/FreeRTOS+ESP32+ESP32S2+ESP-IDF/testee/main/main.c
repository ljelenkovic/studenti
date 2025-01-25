/**
 * Program za testirani sustav
 *
 * Testira se vrijeme odziva na vanjski zahtjev
 * => čim se detektira zahtjev šalje se odgovor
 *
 * koristi se ESP32 (ESP32 chip (via ESP-PROG), UART za Flash Method)
 *
 * GPIO pinovi: (postavljeni u Kconfig.projbuild; tamo ih i mijenjati po potrebi)
 * - REQUEST_PIN  = input, pulled up, interrupt from rising edge (default GPIO5)
 * - RESPONSE_PIN = output (default GPIO18)
 * - LED_PIN      = output -- samo radi testiranja (default GPIO19)
 *   ako je samo jedan sklop, može se spojiti LED_PIN => REQUEST_PIN
 *   i testirati kod
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

/* Načini rada */
#define TEST_MODE_INTERRUPT  1
#define TEST_MODE_THREAD     2
#define TEST_MODE            TEST_MODE_INTERRUPT

static const char *TAG = "testee";

#define REQUEST_PIN          CONFIG_REQUEST_PIN
#define RESPONSE_PIN         CONFIG_RESPONSE_PIN
#define LED_PIN              CONFIG_LED_PIN

#define GPIO_INPUT_PIN_SEL   ((1ULL<<REQUEST_PIN))
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<CONFIG_RESPONSE_PIN) | (1ULL<<CONFIG_LED_PIN))
#define ESP_INTR_FLAG_DEFAULT 0

#define HIGH 1
#define LOW  0

#define BACKGROUND_TASKS  0
#define WD_TIMEOUT	500000 //500 ms - da dretva bar jednom u tom intervalu spava
			//da neki watchdog timer ne prekine tu dretvu
			//možda postaviti na veću vrijednost?

///////////////////////////////////////////////////////////////////////////////
#if TEST_MODE == TEST_MODE_INTERRUPT
//kad dođe zahtjev onda se:
//1. šalje odgovor - preko drugog pina (odmah, bez "odlaganja")
//2. postavlja timer da za 10 ms ugasi odgovor za zahtjev (spisti stanje pina)

TimerHandle_t xTimer;

static void IRAM_ATTR handleRequestInterrupt(void* arg)
{
	gpio_set_level(RESPONSE_PIN, HIGH); //odgovori na zahtjev

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
static void resetResponsePinTimer(TimerHandle_t xTimer)
{
	gpio_set_level(RESPONSE_PIN, LOW); //ugasi odgovor
}

static void setup_interrupt_mode()
{
	//postavi pin za ulaz
	gpio_config_t io_conf = {};

	io_conf.intr_type = GPIO_INTR_POSEDGE; //prekidi, na brid 0->1
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);

	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(REQUEST_PIN, handleRequestInterrupt, NULL);

	xTimer = xTimerCreate("Timer", pdMS_TO_TICKS(10), pdFALSE,
			(void *) 0, resetResponsePinTimer);
}
#endif //TEST_MODE_INTERRUPT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
#if TEST_MODE == TEST_MODE_THREAD
TaskHandle_t RequestProcessingTaskHandle;

void RequestProcessingTask(void *pvParameters)
{
	int64_t last = esp_timer_get_time();

	for (;;) {
		if (gpio_get_level(REQUEST_PIN) == HIGH) {   //provjera zahtjeva
			gpio_set_level(RESPONSE_PIN, HIGH); //slanje odgovora

			//ESP_LOGI(TAG, "dobio zahtjev");

			//ostavi odgovor postavljen 10 ms i onda ga ugasi
			vTaskDelay(pdMS_TO_TICKS(10));
			gpio_set_level(RESPONSE_PIN, LOW);

			last = esp_timer_get_time();
		}
		else {
			//da ne bude predugo bez sleepa ako je period velik
			//jer se onda aktivira neki watchdog timer
			if (esp_timer_get_time() - last > WD_TIMEOUT) {
				//ESP_LOGI(TAG, "spavam");
				vTaskDelay(1);
				last = esp_timer_get_time();
			}
		}
	}
}
void setup_thread_mode()
{
	//postavi pin za ulaz
	gpio_config_t io_conf = {};

	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);

	xTaskCreate(RequestProcessingTask, "Handle Request", 2048, NULL,
		configMAX_PRIORITIES - 1, &RequestProcessingTaskHandle);
}

#endif //TEST_MODE_THREAD
///////////////////////////////////////////////////////////////////////////////

void app_main(void)
{
	ESP_LOGI(TAG, "Inicijalizacija");

	//postavi pinove za izlaz
	gpio_config_t io_conf = {};

	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.mode = GPIO_MODE_OUTPUT;
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 0;
	gpio_config(&io_conf);

	gpio_set_level(RESPONSE_PIN, LOW);

#if TEST_MODE == TEST_MODE_INTERRUPT
	setup_interrupt_mode();
#elif TEST_MODE == TEST_MODE_THREAD
	setup_thread_mode();
#else
	#error TEST_MODE nije TEST_MODE_INTERRUPT ni TEST_MODE_THREAD
#endif

#if BACKGROUND_TASKS > 0
	setup_background_tasks();
#endif

	ESP_LOGI(TAG, "Inicijalizacija gotova, spreman za zahtjeve");
}
