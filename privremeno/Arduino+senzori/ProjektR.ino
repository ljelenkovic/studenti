#include <DallasTemperature.h>
#include <OneWire.h>
#include <TM1637Display.h>

const uint8_t Celsius[] = {
  SEG_A | SEG_B | SEG_F | SEG_G, //°
  SEG_A | SEG_D | SEG_F | SEG_E  //C
};

#define RELAY_PIN 5  // SIG pin za relay
#define ONE_WIRE_BUS 7 // pin za termometar
#define CLK 2
#define DIO 3
#define BUTTON_PIN 13

#define GREEN_LED 8
#define YELLOW_LED 9
#define RED_LED 10

#define TEMP_THRESHOLD 30
 
OneWire oneWire(ONE_WIRE_BUS); //postavljanje termometra
DallasTemperature sensors(&oneWire);
TM1637Display display(CLK, DIO); //postavlanje displaya

bool isRelayOn = false;
bool isManualMode = false;
bool lastButtonState = LOW;
unsigned long buttonPressStartTime = 0;
const unsigned long interval = 3000;

void setup() {
  pinMode(GREEN_LED,OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);  // relay pin na output
  pinMode(BUTTON_PIN, INPUT);
  digitalWrite(RELAY_PIN, LOW);  // relay off


  Serial.begin(9600);
  sensors.begin();
  display.setBrightness(5);  
}

void loop() {
  handleButton();
  float temperature = measureAndDisplayTemperature();
  if (!isManualMode){
    controlRelay(temperature);
  }
  if(temperature >= 40 || temperature < -10){
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  }
  else if(temperature < 40 && temperature >= 30 || temperature >= -10 && temperature < 5){
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  }
  else if(temperature < 30 && temperature >= 5 ){
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);
  }
}

void handleButton() {
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Gumb je stisnut a prethodno stanje je bilo LOW
  if (currentButtonState == HIGH && lastButtonState == LOW) {
    buttonPressStartTime = millis(); 
  }
  // Gumb stisnut 3 sekunde
  if (currentButtonState == HIGH && (millis() - buttonPressStartTime) >= interval) {
    if (!isManualMode) {
      isManualMode = true;
      Serial.println("Entering Manual Mode");
    } else {
      isManualMode = false;
      Serial.println("Exiting Manual Mode");
    }
    while (digitalRead(BUTTON_PIN) == HIGH);
  }
  if (currentButtonState == LOW && lastButtonState == HIGH && isManualMode) {
    isRelayOn = !isRelayOn;
    digitalWrite(RELAY_PIN, isRelayOn ? HIGH : LOW);
  }
  lastButtonState = currentButtonState;
}

int measureAndDisplayTemperature() {
  sensors.requestTemperatures(); 
  float temperature = sensors.getTempCByIndex(0);
  
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" °C");

  int displayTemperature = round(temperature);
  display.showNumberDec(displayTemperature, false, 2, 0);
  display.setSegments(Celsius, 2, 2);
  
  return temperature;
}

void controlRelay(float temperature){
  if (temperature > TEMP_THRESHOLD && !isRelayOn){
    digitalWrite(RELAY_PIN, HIGH);
    isRelayOn = true;
    Serial.println("Relay ON");
  }
  else if (temperature <= TEMP_THRESHOLD && isRelayOn){
    digitalWrite(RELAY_PIN, LOW);
    isRelayOn = false;
    Serial.println("Relay OFF");
  }
}



