# Program za testiranje

Testira se vrijeme odziva na vanjski zahtjev
= ovaj sustav šalje zahjtve i mjeri vrijeme odziva

koristi se ESP32S2 (FIXME ESP32 chip (via ESP-PROG), UART za Flash Method)

GPIO pinovi: (postavljeni u Kconfig.projbuild; tamo ih i mijenjati po potrebi)
- REQUEST_PIN  = input, pulled up, interrupt from rising edge (default GPIO5)
- RESPONSE_PIN = output (default GPIO18)
- LED_PIN      = output -- samo radi testiranja (default GPIO19) ako je samo jedan sklop, može se spojiti LED_PIN => REQUEST_PIN i testirati kod

Za detalje pogledati kod (main/main.c)

(Za upute kako koristiti gpio pogledati
 https://github.com/espressif/esp-idf/tree/master/examples/peripherals/gpio/generic_gpio)

