//Program za ispitivanje odziva drugog sustava

#define REQUEST_PIN  7       // pin za slanje zahtjeva
#define RESPONSE_PIN 8       // pin za čitanje odgovora

#define TEST_MODE_INTERRUPT  1
#define TEST_MODE_THREAD     2
#define TEST_MODE            TEST_MODE_THREAD

#define TEST_PERIOD_MS 100   //svako koliko provoditi slanje zahtjeva
#define TESTS 50             //koliko ukupno testova napraviti

long t_req[TESTS], t_resp[TESTS];
int test = 0;

TaskHandle_t ResponseTestTaskHandle;

//moglo bi i bez dretve, sve staviti u "loop", ali ovo je prikladnije za RTOS
void ResponseTestTask(void *pvParameters)
{
	vTaskDelay(pdMS_TO_TICKS(1000)); //možda malo pričekati prije testova

	for (; test < TESTS; test++)
	{
		//Serial.println("saljem");
		t_resp[test] = 0;
		t_req[test] = micros();
		digitalWrite(REQUEST_PIN, HIGH); //generiranje zahtjeva
		//t_req[test] = micros(); //nije isto kao i gore!? s ovim je 2-3 usec brže

#if TEST_MODE == TEST_MODE_THREAD
		int timeout = 0;

		//Serial.println("Cekam");
		long t = micros();
		while (digitalRead(RESPONSE_PIN) == LOW) {
			t = micros();
			if (t > t_req[test] + TEST_PERIOD_MS) {
				//Serial.println("Response timeout!");
				timeout = 1;
				break;
			}
		}

		if (!timeout)
			t_resp[test] = t;

		digitalWrite(REQUEST_PIN, LOW);
#endif //TEST_MODE_THREAD

		vTaskDelay(pdMS_TO_TICKS(TEST_PERIOD_MS)); //vrijeme mirovanja (perioda)
	}

	//izračunaj statistiku
	int odgovora = 0;
	long max_obrada = 0, min_obrada = TEST_PERIOD_MS;
	double obrada = 0.0;

	for (int i = 0; i < TESTS; i++)
	{
		if (t_resp[i] != 0) {
			long t = t_resp[i] - t_req[i];
			obrada += t;
			if (t > max_obrada)
				max_obrada = t;
			if (t < min_obrada)
				min_obrada = t;
			odgovora++;
			Serial.print("odgovor[");
			Serial.print(i);
			Serial.print("]: ");
			Serial.println(t);
		}
	}

	if (odgovora > 0)
		obrada = obrada / odgovora;

	Serial.print("avg: ");
	Serial.println(obrada);
	Serial.print("min: ");
	Serial.println(min_obrada);
	Serial.print("max: ");
	Serial.println(max_obrada);
	Serial.print("timeouts: ");
	Serial.println(TESTS - odgovora);

	vTaskSuspend(NULL);
}

#if TEST_MODE == TEST_MODE_INTERRUPT
void irqResponseArrived()
{
	t_resp[test] = micros();
	digitalWrite(REQUEST_PIN, LOW);
}
void setup_interrupt_mode()
{
	//odgovor se bilježi u obradi prekida
	attachInterrupt(digitalPinToInterrupt(RESPONSE_PIN),
			irqResponseArrived, RISING);

	//zahtjev šalje dretva
	xTaskCreate(ResponseTestTask, "ResponseTestTask", 1024, NULL,
		configMAX_PRIORITIES - 1, &ResponseTestTaskHandle);
}
#endif //TEST_MODE_INTERRUPT

#if TEST_MODE == TEST_MODE_THREAD
void setup_thread_mode()
{
	//zahtjev šalje dretva koja i čeka na odgovor
	xTaskCreate(ResponseTestTask, "ResponseTestTask", 1024, NULL,
		configMAX_PRIORITIES - 1, &ResponseTestTaskHandle);
}
#endif //TEST_MODE_THREAD

void setup() {
	Serial.begin(115200);
	Serial.println("Inicijalizacija");

	pinMode(RESPONSE_PIN, INPUT); //ilaz za odgovore
	pinMode(REQUEST_PIN, OUTPUT); //izlaz za zahtjeve
	digitalWrite(REQUEST_PIN, LOW);

#if TEST_MODE == TEST_MODE_INTERRUPT
	setup_interrupt_mode();
#elif TEST_MODE == TEST_MODE_THREAD
	setup_thread_mode();
#else
	#error TEST_MODE nije TEST_MODE_INTERRUPT ni TEST_MODE_THREAD
#endif

	Serial.println("Inicijalizacija gotova, pokrecem testiranje");
}

void loop()
{
	//RTOS preuzima kontrolu; loop ostaje prazan
}








