#ifndef THUNDER_DETECTOR_PINS_HPP
#define THUNDER_DETECTOR_PINS_HPP

//Mic
#define PDM_CLK 42
#define PDM_DATA 41

//Camera
#define CAM_PIN_PWDN     -1
#define CAM_PIN_RESET    -1
#define CAM_PIN_XCLK     10
#define I2C_CAM_SDA     40
#define I2C_CAM_SCL     39
#define CAM_PIN_D7       48
#define CAM_PIN_D6       11
#define CAM_PIN_D5       12
#define CAM_PIN_D4       14
#define CAM_PIN_D3       16
#define CAM_PIN_D2       18
#define CAM_PIN_D1       17
#define CAM_PIN_D0       15
#define CAM_PIN_VSYNC    38
#define CAM_PIN_HREF     47
#define CAM_PIN_PCLK     13

//SD
#define SPI_MISO 8
#define SPI_MOSI 9
#define SPI_CLK 7
#define SD_SPI_CS 21

//Misc
#define LED_PIN 21

#endif //THUNDER_DETECTOR_PINS_HPP
