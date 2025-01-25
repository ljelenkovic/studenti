/**
 * Program za testiranje
 *
 * Testira se vrijeme odziva na vanjski zahtjev
 * => ovaj sustav šalje zahjtve i mjeri vrijeme odziva
 *
 * koristi se ESP32S2 (ESP32-S2 chip (via ESP-PROG), UART za Flash Method)
 *
 * GPIO pinovi: (postavljeni u Kconfig.projbuild; tamo ih i mijenjati po potrebi)
 * - REQUEST_PIN  = output (default GPIO7)
 * - RESPONSE_PIN = input, pulled up, interrupt from rising edge (default GPIO8)
 * - LED_PIN      = output -- samo radi testiranja (default GPIO9)
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

static const char *TAG = "tester";

#define REQUEST_PIN          CONFIG_REQUEST_PIN
#define LED_PIN              CONFIG_LED_PIN
#define RESPONSE_PIN         CONFIG_RESPONSE_PIN

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<REQUEST_PIN) | (1ULL<<CONFIG_LED_PIN))
#define GPIO_INPUT_PIN_SEL   ((1ULL<<RESPONSE_PIN))
#define ESP_INTR_FLAG_DEFAULT 0

#define HIGH 1
#define LOW  0

/* Načini rada */
#define TEST_MODE_INTERRUPT  1
#define TEST_MODE_THREAD     2
#define TEST_MODE            TEST_MODE_INTERRUPT

#define TEST_PERIOD_MS  100  //svako koliko provoditi slanje zahtjeva
#define TESTS           50   //koliko ukupno testova napraviti

int64_t t_req[TESTS], t_resp[TESTS];
int test = 0;

TaskHandle_t ResponseTestTaskHandle;

static void ResponseTestTask(void *pvParameters)
{
	vTaskDelay(pdMS_TO_TICKS(1000)); //možda malo pričekati prije testova

	for (; test < TESTS; test++)
	{
		//ESP_LOGI(TAG, "saljem");
		t_resp[test] = 0;
		t_req[test] = esp_timer_get_time();
		gpio_set_level(REQUEST_PIN, HIGH); //generiranje zahtjeva
		//t_req[test] = esp_timer_get_time();

		#if TEST_MODE == TEST_MODE_THREAD
		int timeout = 0;

		//ESP_LOGI(TAG, "Cekam");
		int64_t t = esp_timer_get_time();
		while (gpio_get_level(RESPONSE_PIN) == LOW) {
			t = esp_timer_get_time();
			if (t > t_req[test] + TEST_PERIOD_MS) {
				//ESP_LOGI(TAG, "Response timeout!");
				timeout = 1;
				break;
			}
		}

		if (!timeout)
			t_resp[test] = t;

		gpio_set_level(REQUEST_PIN, LOW);
		#endif //TEST_MODE_THREAD

		vTaskDelay(pdMS_TO_TICKS(TEST_PERIOD_MS)); //vrijeme mirovanja (perioda)
	}

	#if TEST_MODE == TEST_MODE_INTERRUPT
	gpio_isr_handler_remove(RESPONSE_PIN);
	#endif //TEST_MODE_INTERRUPT

	ESP_LOGI(TAG, "Testiranje gotovo, izracunavam statistiku");
	//izračunaj statistiku
	int odgovora = 0;
	int64_t max_obrada = 0, min_obrada = TEST_PERIOD_MS;
	double obrada = 0.0;

	for (int i = 0; i < TESTS; i++)
	{
		if (t_resp[i] != 0) {
			int64_t t = t_resp[i] - t_req[i];
			obrada += t;
			if (t > max_obrada)
				max_obrada = t;
			if (t < min_obrada)
				min_obrada = t;
			odgovora++;
			ESP_LOGI(TAG, "odgovor[%d]: %lld", i, t);
			vTaskDelay(1);//inače se ruši kad koristim prekide!?
		}
	}

	if (odgovora > 0)
		obrada = obrada / odgovora;

	ESP_LOGI(TAG, "avg: %g", obrada);
	ESP_LOGI(TAG, "min: %lld", min_obrada);
	ESP_LOGI(TAG, "max: %lld", max_obrada);
	ESP_LOGI(TAG, "timeouts: %d", TESTS - odgovora);

	vTaskSuspend(NULL);
}


#if TEST_MODE == TEST_MODE_INTERRUPT
static void IRAM_ATTR irqResponseArrived(void* arg)
{
	t_resp[test] = esp_timer_get_time();
	gpio_set_level(REQUEST_PIN, LOW); //ugasi zahtjev
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
	gpio_isr_handler_add(RESPONSE_PIN, irqResponseArrived, NULL);

	//zahtjev šalje dretva
	xTaskCreate(ResponseTestTask, "ResponseTestTask", 1024, NULL,
		configMAX_PRIORITIES - 1, &ResponseTestTaskHandle);
}
#endif //TEST_MODE_INTERRUPT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
#if TEST_MODE == TEST_MODE_THREAD
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

	xTaskCreate(ResponseTestTask, "Handle Request", 2048, NULL,
		configMAX_PRIORITIES - 1, &ResponseTestTaskHandle);
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

	gpio_set_level(REQUEST_PIN, LOW);

#if TEST_MODE == TEST_MODE_INTERRUPT
	setup_interrupt_mode();
#elif TEST_MODE == TEST_MODE_THREAD
	setup_thread_mode();
#else
	#error TEST_MODE nije TEST_MODE_INTERRUPT ni TEST_MODE_THREAD
#endif

	ESP_LOGI(TAG, "Inicijalizacija gotova, pokrecem testiranje");
}
