//Program za testirani sustav - testira se vrijeme odziva na vanjski zahtjev
//- čim se detektira zahtjev šalje se odgovor

#define REQUEST_PIN  4 //pin na koji stiže zahtjev
#define RESPONSE_PIN 2 //pin za signalizaciju završetka obrade

#define TEST_MODE_INTERRUPT  1
#define TEST_MODE_THREAD     2
#define TEST_MODE            TEST_MODE_INTERRUPT

#define BACKGROUND_TASKS  0 //koliko zadataka u pozadini kao opterećenje
//hmm, izgleda da "arduino sustav/način rada" hoće da sustav ima vremena za "idle" dretve
//ako nema onda wdt "opali"
//kad koristim prekide, onda i s dvije dretve radi ok
//kad koristim dretvu, onda radi s jednom dretvom ok
//s više dretvi dođe do wdt restarta
//pitanje je onda imaju li ove dretve u pozadini smisla
//možda ako se koristi IDF okruženje, tamo će možda biti OK?
//uglavnom, čini mi se da dretve u pozadini uopće ne smetaju ako imaju manji prioritet
//što bi i očekivao od RTOS-a :)

#define WD_TIMEOUT  500000 //500 ms - da dretva bar jednom u tom intervalu spava
			   //da neki watchdog timer ne prekine tu dretvu
			   //možda postaviti na veću vrijednost?

///////////////////////////////////////////////////////////////////////////////
#if TEST_MODE == TEST_MODE_INTERRUPT
//kad dođe zahtjev onda se:
//1. šalje odgovor - preko drugog pina (odmah, bez "odlaganja")
//2. postavlja timer da za 10 ms ugasi odgovor za zahtjev (spisti stanje pina)

TimerHandle_t xTimer;

void handleRequestInterrupt() //ne treba IRAM_ATTR?
{
	digitalWrite(RESPONSE_PIN, HIGH); //odgovor na zahtjev

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
void resetResponsePinTimer(TimerHandle_t xTimer)
{
	digitalWrite(RESPONSE_PIN, LOW); //ugasi odgovor
}

void setup_interrupt_mode()
{
	attachInterrupt(digitalPinToInterrupt(REQUEST_PIN),
		handleRequestInterrupt, RISING);
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
	long last = micros();

	for (;;) {
		if (digitalRead(REQUEST_PIN) == HIGH) {   //provjera zahtjeva
			digitalWrite(RESPONSE_PIN, HIGH); //slanje odgovora

			//Serial.println("dobio zahtjev");

			//ostavi odgovor postavljen 10 ms i onda ga ugasi
			vTaskDelay(pdMS_TO_TICKS(10));
			digitalWrite(RESPONSE_PIN, LOW);

			last = micros();
		}
		else {
			//da ne bude predugo bez sleepa ako je period velik
			//jer se onda aktivira neki watchdog timer
			if (micros() - last > WD_TIMEOUT) {
				//Serial.println("spavam");
				vTaskDelay(1);
				last = micros();
			}
		}
	}
}
void setup_thread_mode()
{
	xTaskCreate(RequestProcessingTask, "Handle Request", 1024, NULL,
		configMAX_PRIORITIES - 1, &RequestProcessingTaskHandle);
}

#endif //TEST_MODE_THREAD
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
#if BACKGROUND_TASKS > 0
TaskHandle_t backgroundTaskHandles[BACKGROUND_TASKS];
long period = 1000;

void background_loop()
{
	for (long i = 0; i < period; i++) {
		asm volatile("":::"memory");
	}
}
void backgroundTask(void *pvParameters)
{
	long periods = (long) pvParameters;

	Serial.print("Krece dretva ");
	Serial.println(periods);

	long last = micros(); //da ne bude predugo bez sleepa
	for (;;) {
		//for (long i = 0; i < periods; i++) {
			background_loop();
		//}

		if (micros() - last > WD_TIMEOUT) {
			//vTaskDelay(pdMS_TO_TICKS(1));
			vTaskDelay(1);
			last = micros();
		}
	}
}
void set_period()
{
	long t0 = 0, t1 = 0;

	while (t1 - t0 < 10000) { //barem 10 ms
		period *= 2;
		t0 = micros();
		background_loop();
		t1 = micros();
	}
}
void setup_background_tasks()
{
	set_period();
	for (int i = 0; i < BACKGROUND_TASKS; i++)
		xTaskCreate(backgroundTask, "Background Task", 1024,
			(void *) (i+1), 1, &backgroundTaskHandles[i]);

	//ovo baš i ne radi
	//disableCore0WDT();
	//disableCore1WDT();
}
#endif //BACKGROUND_TASKS > 0
///////////////////////////////////////////////////////////////////////////////

void setup()
{
	Serial.begin(115200);
	Serial.println("Inicijalizacija");

	pinMode(REQUEST_PIN, INPUT);   //Ulaz za zahtjeve
	pinMode(RESPONSE_PIN, OUTPUT); //Izlaz za signalizaciju
	digitalWrite(RESPONSE_PIN, LOW);

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

	Serial.println("Inicijalizacija gotova, spreman za zahtjeve");
}

void loop()
{
	//RTOS preuzima kontrolu; loop ostaje prazan
}
