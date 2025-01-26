#include "VisualDetector.h"
#include "Util/Timer.h"
#include <esp_log.h>

#undef EPS

#include <opencv2/imgproc.hpp>

static const char* TAG = "VideoDetect";

VisualDetector::VisualDetector(Camera* cam, Queue<SensorEvent>* queue) : Threaded("VideoDetect", 12 * 1024, 5, 0), camera(cam), outputQueue(queue){

	frame0Data = std::unique_ptr<uint8_t>((uint8_t*) heap_caps_malloc(ScaledWidth * ScaledHeight, MALLOC_CAP_SPIRAM));
	frame1Data = std::unique_ptr<uint8_t>((uint8_t*) heap_caps_malloc(ScaledWidth * ScaledHeight, MALLOC_CAP_SPIRAM));

	frame0 = cv::Mat(ScaledHeight, ScaledWidth, CV_8U, frame0Data.get());
	frame1 = cv::Mat(ScaledHeight, ScaledWidth, CV_8U, frame1Data.get());
}

void VisualDetector::loop(){

	const auto start = millis();

	lastShotTimestamp = millis();
	camera_fb_t* frameData = camera->getFrame();
	if(frameData == nullptr || frameData->buf == nullptr || frameData->len == 0){
		ESP_LOGE(TAG, "Camera getFrame fail!");
		camera->releaseFrame();
		return;
	}

	const auto frameGet = millis() - start;

	const auto intensity = detectLightning(frameData);

	if(intensity > 0){
		storeShots();
		if(outputQueue){
			SensorEvent event{ SensorEvent::Type::Video, lastShotTimestamp, { .video = { (uint8_t) intensity }}};
			outputQueue->post(event, portMAX_DELAY);
		}
	}

	const auto total = millis() - start;
	const auto detection = total - frameGet;

	ESP_LOGD(TAG, "frame get: %llums, detection: %llums, total: %llums, fps: %.2f", frameGet, detection, total, 1000.0f / (float) total);

	camera->releaseFrame();

//	vTaskDelay(50);

}

int VisualDetector::detectLightning(camera_fb_t* frameData){
	const uint8_t* rawFrame = frameData->buf;
	std::vector<uint8_t> grayFrame(FrameWidth * FrameHeight);

	//RGB565 format to 8-bit grayscale
	for(size_t i = 0; i < grayFrame.size(); ++i){
		uint16_t color = ((uint16_t*) rawFrame)[i];
		color = (color >> 8) | (color << 8);

		uint8_t r = (color & 0xF800) >> 11;
		uint8_t g = (color & 0x07E0) >> 5;
		uint8_t b = (color & 0x001F);

		r = std::clamp((r * 255) / 31, 0, 255);
		g = std::clamp((g * 255) / 63, 0, 255);
		b = std::clamp((b * 255) / 31, 0, 255);

		grayFrame[i] = (r + g + b) / 3;
	}


	const cv::Mat gray(FrameHeight, FrameWidth, CV_8U, grayFrame.data());

	//Need to fill initial frame buffer to start comparison
	if(!initialFill){
		initialFill = true;
		cv::resize(gray, frame0, frame0.size(), 0, 0, cv::InterpolationFlags::INTER_LINEAR);
		return 0;
	}

	cv::resize(gray, frame1, frame1.size(), 0, 0, cv::InterpolationFlags::INTER_LINEAR);

	std::vector<uint8_t> diffFrame(ScaledWidth * ScaledHeight);
	cv::Mat diff(ScaledHeight, ScaledWidth, CV_8U, diffFrame.data());
	cv::absdiff(frame0, frame1, diff);

	cv::Mat denoisedDiff(ScaledHeight, ScaledWidth, CV_8U);
	cv::threshold(diff, denoisedDiff, NoiseCutoff, 255, cv::ThresholdTypes::THRESH_TOZERO);

	const auto count = cv::countNonZero(denoisedDiff);
	ESP_LOGD(TAG, "Diff pixel count: %d", count);

	cv::swap(frame0, frame1);

	if(count < DetectionPixelNum) return 0;


	const auto sum = cv::sum(denoisedDiff);
	return (int) (sum[0] / count);
}

void VisualDetector::storeShots(){
	uint8_t* out;
	size_t len;

	if(!fmt2jpg(frame0.data, frame0.cols * frame0.rows, frame0.cols, frame0.rows, PIXFORMAT_GRAYSCALE, 30, &out, &len)){
		ESP_LOGE(TAG, "frame2jpg conversion failed.");
	}

	std::string name_b = "/sd/" + std::to_string(lastShotTimestamp) + "_b.jpg";
	std::string name_a = "/sd/" + std::to_string(lastShotTimestamp) + "_a.jpg";

	FILE* file = fopen(name_b.c_str(), "w");
	if(!file){
		ESP_LOGE(TAG, "error opening file on SD!\n");
	}
	size_t written = fwrite(out, 1, len, file);
	ESP_LOGD(TAG, "written %d to %s\n", written, name_b.c_str());

	fclose(file);

	free(out);

	if(!fmt2jpg(frame1.data, frame1.cols * frame1.rows, frame1.cols, frame1.rows, PIXFORMAT_GRAYSCALE, 30, &out, &len)){
		ESP_LOGE(TAG, "frame2jpg conversion failed.");
	}

	file = fopen(name_a.c_str(), "w");
	if(!file){
		ESP_LOGE(TAG, "error opening file on SD!\n");
	}
	written = fwrite(out, 1, len, file);
	ESP_LOGD(TAG, "written %d to %s\n", written, name_a.c_str());

	fclose(file);

	free(out);
}
