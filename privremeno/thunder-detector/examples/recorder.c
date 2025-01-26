/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/* I2S Digital Microphone Recording Example */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Header structure for WAV file with only one data chunk
 *
 * @note See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
 *
 * @note Assignment to variables in this struct directly is only possible for little endian architectures
 *       (including Xtensa & RISC-V)
 */
typedef struct {
	struct {
		char chunk_id[4]; /*!< Contains the letters "RIFF" in ASCII form */
		uint32_t chunk_size; /*!< This is the size of the rest of the chunk following this number */
		char chunk_format[4]; /*!< Contains the letters "WAVE" */
	} descriptor_chunk; /*!< Canonical WAVE format starts with the RIFF header */
	struct {
		char subchunk_id[4]; /*!< Contains the letters "fmt " */
		uint32_t subchunk_size; /*!< This is the size of the rest of the Subchunk which follows this number */
		uint16_t audio_format; /*!< PCM = 1, values other than 1 indicate some form of compression */
		uint16_t num_of_channels; /*!< Mono = 1, Stereo = 2, etc. */
		uint32_t sample_rate; /*!< 8000, 44100, etc. */
		uint32_t byte_rate; /*!< ==SampleRate * NumChannels * BitsPerSample s/ 8 */
		uint16_t block_align; /*!< ==NumChannels * BitsPerSample / 8 */
		uint16_t bits_per_sample; /*!< 8 bits = 8, 16 bits = 16, etc. */
	} fmt_chunk; /*!< The "fmt " subchunk describes the sound data's format */
	struct {
		char subchunk_id[4]; /*!< Contains the letters "data" */
		uint32_t subchunk_size; /*!< ==NumSamples * NumChannels * BitsPerSample / 8 */
		int16_t data[0]; /*!< Holds raw audio data */
	} data_chunk; /*!< The "data" subchunk contains the size of the data and the actual sound */
} wav_header_t;

/**
 * @brief Default header for PCM format WAV files
 *
 */
#define WAV_HEADER_PCM_DEFAULT(wav_sample_size, wav_sample_bits, wav_sample_rate, wav_channel_num) { \
    .descriptor_chunk = { \
        .chunk_id = {'R', 'I', 'F', 'F'}, \
        .chunk_size = (wav_sample_size) + sizeof(wav_header_t) - 8, \
        .chunk_format = {'W', 'A', 'V', 'E'} \
    }, \
    .fmt_chunk = { \
        .subchunk_id = {'f', 'm', 't', ' '}, \
        .subchunk_size = 16, /* 16 for PCM */ \
        .audio_format = 1, /* 1 for PCM */ \
        .num_of_channels = (wav_channel_num), \
        .sample_rate = (wav_sample_rate), \
        .byte_rate = (wav_sample_bits) * (wav_sample_rate) * (wav_channel_num) / 8, \
        .block_align = (wav_sample_bits) * (wav_channel_num) / 8, \
        .bits_per_sample = (wav_sample_bits)\
    }, \
    .data_chunk = { \
        .subchunk_id = {'d', 'a', 't', 'a'}, \
        .subchunk_size = (wav_sample_size) \
    } \
}

#ifdef __cplusplus
}
#endif


static const char *TAG = "pdm_rec_example";

//Hardcoded for XIAO ESP32-S3 Sense
#define CONFIG_EXAMPLE_SPI_MISO_GPIO 8
#define CONFIG_EXAMPLE_SPI_MOSI_GPIO 9
#define CONFIG_EXAMPLE_SPI_SCLK_GPIO 7
#define CONFIG_EXAMPLE_SPI_CS_GPIO 21
#define CONFIG_EXAMPLE_SAMPLE_RATE 16000
#define CONFIG_EXAMPLE_BIT_SAMPLE 16
#define CONFIG_EXAMPLE_I2S_DATA_GPIO 41
#define CONFIG_EXAMPLE_I2S_CLK_GPIO 42
#define CONFIG_EXAMPLE_REC_TIME 10


#define SPI_DMA_CHAN        SPI_DMA_CH_AUTO
#define NUM_CHANNELS        (1) // For mono recording only!
#define SD_MOUNT_POINT      "/sdcard"
#define SAMPLE_SIZE         (CONFIG_EXAMPLE_BIT_SAMPLE * 1024)
#define BYTE_RATE           (CONFIG_EXAMPLE_SAMPLE_RATE * (CONFIG_EXAMPLE_BIT_SAMPLE / 8)) * NUM_CHANNELS

// When testing SD and SPI modes, keep in mind that once the card has been
// initialized in SPI mode, it can not be reinitialized in SD mode without
// toggling power to the card.
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdmmc_card_t *card;
i2s_chan_handle_t rx_handle = NULL;

static int16_t i2s_readraw_buff[SAMPLE_SIZE];
size_t bytes_read;
const int WAVE_HEADER_SIZE = 44;

void mount_sdcard(void)
{
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
        .mosi_io_num = CONFIG_EXAMPLE_SPI_MOSI_GPIO,
        .miso_io_num = CONFIG_EXAMPLE_SPI_MISO_GPIO,
        .sclk_io_num = CONFIG_EXAMPLE_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = CONFIG_EXAMPLE_SPI_CS_GPIO;
    slot_config.host_id = host.slot;

    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
}

void record_wav(uint32_t rec_time)
{
    // Use POSIX and C standard library functions to work with files.
    int flash_wr_size = 0;
    ESP_LOGI(TAG, "Opening file");

    uint32_t flash_rec_time = BYTE_RATE * rec_time;
    const wav_header_t wav_header =
        WAV_HEADER_PCM_DEFAULT(flash_rec_time, 16, CONFIG_EXAMPLE_SAMPLE_RATE, 1);

    // First check if file exists before creating a new file.
    struct stat st;
    if (stat(SD_MOUNT_POINT"/record.wav", &st) == 0) {
        // Delete it if it exists
        unlink(SD_MOUNT_POINT"/record.wav");
    }

    // Create new WAV file
    FILE *f = fopen(SD_MOUNT_POINT"/record.wav", "a");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    fwrite(&wav_header, sizeof(wav_header), 1, f);

    // Start recording
    while (flash_wr_size < flash_rec_time) {
        // Read the RAW samples from the microphone
        if (i2s_channel_read(rx_handle, (char *)i2s_readraw_buff, SAMPLE_SIZE, &bytes_read, 1000) == ESP_OK) {
            printf("[0] %d [1] %d [2] %d [3]%d ...\n", i2s_readraw_buff[0], i2s_readraw_buff[1], i2s_readraw_buff[2], i2s_readraw_buff[3]);
            // Write the samples to the WAV file
            fwrite(i2s_readraw_buff, bytes_read, 1, f);
            flash_wr_size += bytes_read;
        } else {
            printf("Read Failed!\n");
        }
    }

    ESP_LOGI(TAG, "Recording done!");
    fclose(f);
    ESP_LOGI(TAG, "File written on SDCard");

    // All done, unmount partition and disable SPI peripheral
    esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, card);
    ESP_LOGI(TAG, "Card unmounted");
    // Deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}

void init_microphone(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(CONFIG_EXAMPLE_SAMPLE_RATE),
        /* The default mono slot is the left slot (whose 'select pin' of the PDM microphone is pulled down) */
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = CONFIG_EXAMPLE_I2S_CLK_GPIO,
            .din = CONFIG_EXAMPLE_I2S_DATA_GPIO,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_rx_mode(rx_handle, &pdm_rx_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
}

void app_main(void)
{
    printf("PDM microphone recording example start\n--------------------------------------\n");
    // Mount the SDCard for recording the audio file
    mount_sdcard();
    // Acquire a I2S PDM channel for the PDM digital microphone
    init_microphone();
    ESP_LOGI(TAG, "Starting recording for %d seconds!", CONFIG_EXAMPLE_REC_TIME);
    // Start Recording
    record_wav(CONFIG_EXAMPLE_REC_TIME);
    // Stop I2S driver and destroy
    ESP_ERROR_CHECK(i2s_channel_disable(rx_handle));
    ESP_ERROR_CHECK(i2s_del_channel(rx_handle));
}
