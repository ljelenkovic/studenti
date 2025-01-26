#ifndef THUNDER_DETECTOR_SD_H
#define THUNDER_DETECTOR_SD_H

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <soc/gpio_num.h>

class SD {
public:
	static bool init(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk, gpio_num_t cs = GPIO_NUM_NC, const char* mountpoint = BasePath);
	static void deinit();

private:
	static constexpr const char* BasePath = "/sd";

	static sdmmc_host_t host;
	static sdmmc_card_t *card;
};


#endif //THUNDER_DETECTOR_SD_H
