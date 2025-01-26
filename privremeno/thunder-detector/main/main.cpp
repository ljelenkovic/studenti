#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "AudioDetector.h"
#include "Periph/I2C.h"
#include "Pins.hpp"
#include "Devices/Camera.h"
#include "VisualDetector.h"
#include "Periph/SD.h"

void init(){
	esp_log_level_set("*", ESP_LOG_INFO);
//	esp_log_level_set("AudioDetect", ESP_LOG_DEBUG);
//	esp_log_level_set("VideoDetect", ESP_LOG_DEBUG);
//	esp_log_level_set("camera", ESP_LOG_DEBUG);

	if(!SD::init((gpio_num_t) SPI_MISO, (gpio_num_t) SPI_MOSI,
				 (gpio_num_t) SPI_CLK, (gpio_num_t) SD_SPI_CS, "/sd")){
		return;
	}

	auto i2c = new I2C(I2C_NUM_0, (gpio_num_t) I2C_CAM_SDA, (gpio_num_t) I2C_CAM_SCL);
	auto camera = new Camera(*i2c);

	if(camera->init() != ESP_OK){
		printf("Cam init error\n");
	}

	Queue<SensorEvent> queue(16);

	auto audio = new AudioDetector(16000, 16000, &queue);
	audio->start();

	auto video = new VisualDetector(camera, &queue);
	video->start();


	bool recognizedVideo = false;
	SensorEvent storedVideo{};
	static constexpr size_t AudioDelayCutoff = 60000; //60 seconds shouldn't be audible/visible
	static constexpr int V_sound = 343; //[m/s], speed of sound constant

	while(1){
		SensorEvent event{};
		if(queue.get(event, portMAX_DELAY)){

			if(event.type == SensorEvent::Type::Audio){

				auto audioEvent = event.audio;
				if(audioEvent.type == ThunderType::Clap){
					printf("Clap at %d ms!\n", event.timestamp);

					if(!recognizedVideo){
						printf("Clap ignored, no preceding video event\n");
						continue;
					}


					const int timeDiff = event.timestamp - storedVideo.timestamp;
					if(timeDiff > AudioDelayCutoff){
						printf("Clap ignored, too much time passed since video event\n");
					}else{
						const float distance = timeDiff * V_sound / 1000.0f; //distance in meters
						printf("Possible thunderstrike detected, distance: %.2f m, timestamp: %d\n", distance, storedVideo.timestamp);
					}

					recognizedVideo = false;
				}
			}else if(event.type == SensorEvent::Type::Video){

				auto videoEvent = event.video;
				if(videoEvent.intensity > 0){
					printf("Video change at %d ms! Waiting for a thunder follow-up...\n", event.timestamp);
					recognizedVideo = true;
					storedVideo = event;
				}

			}
		}
	}

}


extern "C" void app_main(void){
	init();

	vTaskDelete(nullptr);
}
