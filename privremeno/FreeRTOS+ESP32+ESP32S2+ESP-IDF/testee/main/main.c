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
 *   i testirati kod (ali ovo nije testirano!)
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
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "bootloader_random.h"
#include "esp_chip_info.h"

/* Načini rada */
#define TEST_MODE_INTERRUPT  1
#define TEST_MODE_TASK     2
#define TEST_MODE            TEST_MODE_TASK

static const char *TAG = "testee";

#define REQUEST_PIN          CONFIG_REQUEST_PIN
#define RESPONSE_PIN         CONFIG_RESPONSE_PIN
#define LED_PIN              CONFIG_LED_PIN

#define GPIO_INPUT_PIN_SEL   ((1ULL<<REQUEST_PIN))
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<CONFIG_RESPONSE_PIN) | (1ULL<<CONFIG_LED_PIN))
#define ESP_INTR_FLAG_DEFAULT 0

#define HIGH 1
#define LOW  0

#define BACKGROUND_TASKS  4
#define WD_TIMEOUT	500000 //500 ms - da dretva bar jednom u tom intervalu spava
			//da neki watchdog timer ne prekine tu dretvu
			//možda postaviti na veću vrijednost?

///////////////////////////////////////////////////////////////////////////////
#if TEST_MODE == TEST_MODE_INTERRUPT
//kad dođe zahtjev onda se:
//1. šalje odgovor - preko drugog pina (odmah, bez "odlaganja")
//2. postavlja timer da za 10 ms ugasi odgovor za zahtjev (spusti stanje pina)

static TimerHandle_t xTimer;

//obrada prekid (kad na REQUEST_PIN dođe zahtjev)
static void IRAM_ATTR handleRequestInterrupt(void* arg)
{
	//odgovori na zahtjev
	gpio_set_level(RESPONSE_PIN, HIGH);

	//postavi alarm za reset odgovora
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

//funkcija za aktiviranje alarma
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

	//pokreni sustav za prihvat prekida
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

	//registriraj prekidnu funkciju za prekid na pinu
	gpio_isr_handler_add(REQUEST_PIN, handleRequestInterrupt, NULL);

	//stvori alarm (ali neaktivan)
	xTimer = xTimerCreate("Timer", pdMS_TO_TICKS(10), pdFALSE,
			(void *) 0, resetResponsePinTimer);
}
#endif //TEST_MODE_INTERRUPT
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
#if TEST_MODE == TEST_MODE_TASK
static TaskHandle_t RequestProcessingTaskHandle;

//dretva za reakciju na događaj
static void RequestProcessingTask(void *pvParameters)
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

static void setup_thread_mode()
{
	//postavi pin za ulaz
	gpio_config_t io_conf = {};

	io_conf.intr_type = GPIO_INTR_DISABLE; //ne prekidi
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);

	xTaskCreate(RequestProcessingTask, "Handle Request", 4096, NULL,
		configMAX_PRIORITIES - 2, &RequestProcessingTaskHandle);
}
#endif //TEST_MODE_TASK
///////////////////////////////////////////////////////////////////////////////
#if BACKGROUND_TASKS > 0
	static void setup_background_tasks(); //samo deklaracija, kod je ispod
#endif

//početna dretva
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

	//ovoj dretvi najveći prioritet, da joj ostale ne smetaju dok postavlja sve
	vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);

#if BACKGROUND_TASKS > 0
	setup_background_tasks();
#endif

#if TEST_MODE == TEST_MODE_INTERRUPT
	setup_interrupt_mode();
#elif TEST_MODE == TEST_MODE_TASK
	setup_thread_mode();
#else
	#error TEST_MODE nije TEST_MODE_INTERRUPT ni TEST_MODE_TASK
#endif

	ESP_LOGI(TAG, "Inicijalizacija gotova, spreman za zahtjeve");
}

///////////////////////////////////////////////////////////////////////////////
#if BACKGROUND_TASKS > 0
static TaskHandle_t backgroundTaskHandle[BACKGROUND_TASKS];
#define TPERIOD  100000 //100 ms
static int64_t period = 1000;
static int *bgdata = NULL;
static size_t bgdata_size = 0;
static uint8_t cores = 2;

//unutarnja petlja koje izvode dretve u pozadini
static void background_loop()
{
	for (int64_t i = 0; i < period; i++) {
		if (bgdata) {
			//simulirati neki rad
			uint32_t r1 = esp_random() % bgdata_size;
			uint32_t r2 = esp_random() % bgdata_size;
			bgdata[r1] = bgdata[r1] + bgdata[r2] + r1 + r2;
		}
		else {
			asm volatile("":::"memory");
		}
	}
}

//pozadinska dretva
static void backgroundTask(void *pvParameters)
{
	long id = (long) pvParameters;

	long inner_loops = id * 1000000 / TPERIOD;
	if (BACKGROUND_TASKS > 1)
		inner_loops = inner_loops * cores / BACKGROUND_TASKS;
	//Ideja: otprilike svakih 'id' sekundi po jedan ispis za dretvu 'id'
	//Unutarnja petlja traje oko TPERIOD (kada dretva radi na procesoru),
	//pa bi (1000000 / TPERIOD) unutarnjih petlji bila sekunda.
	//Ali obzirom da dretve dijele vrijeme svaka će dobiti "dio"
	//procesorskog vremena, pa je 10 iteracija petlje više od sekunde.
	//Zato je broj petlji skaliran s brojem jezgri i brojem dretvi.

	ESP_LOGI(TAG, "Krece dretva %ld", id);

	for (;;) {
		for (long i = 0; i < inner_loops; i++) {
			background_loop();
			taskYIELD();
		}
		ESP_LOGI(TAG, "Dretva %ld nesto radi", id);
	}
}

//izračunaj 'period' za unutarnju petlju
static void set_period()
{
	int64_t t0 = 0, t1 = 0;

	while (t1 - t0 < TPERIOD) {
		period *= 2;
		t0 = esp_timer_get_time();
		background_loop();
		t1 = esp_timer_get_time();
	}
	//skaliraj period da bude TPERIOD usec rada
	double p = ((double) period) * TPERIOD / (t1 - t0);
	period = (int64_t) p; //double radi mogućeg preljeva
}

static void setup_background_tasks()
{
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
	cores = chip_info.cores;

	esp_task_wdt_deinit(); //da wdt ne prekida sustav

	bgdata_size = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) / 2 / sizeof(int);
	bgdata = heap_caps_calloc(sizeof(int), bgdata_size, MALLOC_CAP_DEFAULT);
	if (bgdata == NULL)
		ESP_LOGI(TAG, "Nema memorije za bgdata");

	bootloader_random_enable();

	set_period();

	for (int i = 0; i < BACKGROUND_TASKS; i++)
		xTaskCreate(backgroundTask, "Background Task", 4096,
			(void *) (i+1), 1, &backgroundTaskHandle[i]);
}
#endif //BACKGROUND_TASKS > 0
///////////////////////////////////////////////////////////////////////////////
