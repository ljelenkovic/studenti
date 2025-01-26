#include <esp_log.h>
#include "AudioDetector.h"
#include "Pins.hpp"
#include "Util/Timer.h"

static const char* TAG = "AudioDetect";

AudioDetector::AudioDetector(size_t bufferSize, uint16_t sampleRate, Queue<SensorEvent>* queue) :
		Threaded("Audio", 8 * 1024, 5, 1), sampleRate(sampleRate), bufferSize(bufferSize), outputQueue(queue){

	buffer = (int16_t*) malloc(bufferSize * sizeof(int16_t));
	i2s_init(sampleRate);
	ESP_LOGD(TAG, "i2s inited");
}

int AudioDetector::i2s_init(uint32_t sampling_rate){
	i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
	ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, nullptr, &rx_chan));


	i2s_pdm_rx_config_t pdm_rx_cfg = {
			.clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sampling_rate),
			.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
			.gpio_cfg = {
					.clk = (gpio_num_t) PDM_CLK,
					.din = (gpio_num_t) PDM_DATA,
					.invert_flags = {
							.clk_inv = false,
					},
			},
	};

	ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_chan, &pdm_rx_cfg));

	ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
	return ESP_OK;
}

void AudioDetector::loop(){
	size_t bytesRead = 0;

	size_t startMillis = millis();
//	ESP_LOGD(TAG, "Start block recording, currentVal: %d", currentValue);
	auto ret = i2s_channel_read(rx_chan, (void*) buffer, bufferSize * sizeof(int16_t), &bytesRead, portMAX_DELAY);

	if(ret != ESP_OK) return;

	if(calibrationSample){
		currentValue = buffer[0];
		for(size_t i = 0; i < bytesRead; i++){
			const auto& sample = buffer[i];
			currentValue = currentValue * (1.0f - EMAFactor) + EMAFactor * sample;
		}
		calibrationSample = false;
		return;
	}

	detectClap(startMillis);
}

void AudioDetector::detectClap(size_t startTime){
	for(size_t i = 0; i < bufferSize; i++){
		const auto& sample = buffer[i];

		switch(clapState){
			case None:
				if(abs(currentValue - sample) > ClapSpikeThreshold){
					clapState = SpikeDetected;
					spikeTimestamp = startTime + samplesToMs(i);
					prevDecayDiff = abs(currentValue - sample);
					ESP_LOGD(TAG, "Spike found at time %zu\n", spikeTimestamp);
				}
				break;

			case SpikeDetected:{
				const auto diff = abs(currentValue - sample);
				const auto decayDoneTimestamp = startTime + samplesToMs(i);

				if(diff > prevDecayDiff){
					ESP_LOGD(TAG, "Decay didn't occur");
					clapState = None;
					spikeTimestamp = 0;

				}else if(decayDoneTimestamp - spikeTimestamp >= ClapDecayTimeout && abs(currentValue - sample) < ClapDecayThreshold){
					//decay after spike - proper clap
					ESP_LOGD(TAG, "Decay after spike found!");
					if(outputQueue){
						SensorEvent event{ SensorEvent::Type::Audio, spikeTimestamp, { .audio = { ThunderType::Clap }}};
						bool ret = outputQueue->post(event, 0);
						if(!ret){
							ESP_LOGE(TAG, "Output queue is full!");
						}
					}
					clapState = None;
					spikeTimestamp = 0;
				}
				break;
			}

			case DecayDetected:
				break;
		}

		currentValue = currentValue * (1.0f - EMAFactor) + EMAFactor * sample;
	}
}

void AudioDetector::detectPeal(){
}

void AudioDetector::detectRumble(){
}
