#include "SD.h"

static const char* TAG = "SD";

sdmmc_host_t SD::host = SDSPI_HOST_DEFAULT();
sdmmc_card_t* SD::card = nullptr;

bool SD::init(gpio_num_t miso, gpio_num_t mosi, gpio_num_t clk, gpio_num_t cs, const char* mountpoint){
	esp_err_t ret;
	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
			.format_if_mount_failed = true,
			.max_files = 5,
			.allocation_unit_size = 8 * 1024
	};
	ESP_LOGI(TAG, "Initializing SD card");

	spi_bus_config_t bus_cfg = {
			.mosi_io_num = mosi,
			.miso_io_num = miso,
			.sclk_io_num = clk,
			.quadwp_io_num = -1,
			.quadhd_io_num = -1,
			.max_transfer_sz = 4000,
	};
	ret = spi_bus_initialize((spi_host_device_t) host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
	if(ret != ESP_OK){
		ESP_LOGE(TAG, "Failed to initialize bus.");
		return false;
	}

	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot_config.gpio_cs = cs;
	slot_config.host_id = (spi_host_device_t) host.slot;

	ret = esp_vfs_fat_sdspi_mount(BasePath, &host, &slot_config, &mount_config, &card);

	if(ret != ESP_OK){
		if(ret == ESP_FAIL){
			ESP_LOGE(TAG, "Failed to mount filesystem.");
		}else{
			ESP_LOGE(TAG, "Failed to initialize the card (%s). "
						  "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
		}
		return false;
	}

	// Card has been initialized, print its properties
	sdmmc_card_print_info(stdout, card);

	return true;
}

void SD::deinit(){
	esp_vfs_fat_sdcard_unmount(BasePath, card);
}
