#ifndef THUNDER_DETECTOR_VISUALDETECTOR_H
#define THUNDER_DETECTOR_VISUALDETECTOR_H

#include "Util/Threaded.h"
#include "Devices/Camera.h"
#include "Periph/SD.h"
#include "Util/Queue.h"
#include "SensorEvent.hpp"

#undef EPS

#include <opencv2/core/mat.hpp>


class VisualDetector : public Threaded {
public:
	VisualDetector(Camera* cam, Queue<SensorEvent>* queue = nullptr);

protected:
	void loop() override;

private:
	Camera* camera;
	Queue<SensorEvent>* outputQueue = nullptr;
	bool initialFill = false;

	cv::Mat frame0, frame1;
	std::unique_ptr<uint8_t> frame0Data, frame1Data;
	size_t lastShotTimestamp = 0;

	/**
	 * @param frameData
	 * @return 0 if none detected, otherwise a positive integer that indicates the change intensity
	 */
	int detectLightning(camera_fb_t* frameData);

	void storeShots();


	//Noise cutoff when determining difference between frames
	static constexpr uint8_t NoiseCutoff = 10;

	//Scale factor when calculating the difference between frames (for memory and time efficiency)
	static constexpr float Scale = 1.0f;

	/**
	 * Percentage of image pixels that need to change between two frames to indicate a lightning strike
	 * Should be in approximate range of 0.2 - 0.01
	 */
	static constexpr float DetectionThreshold = 0.1f;

	static constexpr uint32_t FrameWidth = 160, FrameHeight = 120; //160x120 resolution
	static constexpr uint32_t ScaledWidth = FrameWidth * Scale;
	static constexpr uint32_t ScaledHeight = FrameHeight * Scale;

	static constexpr uint32_t DetectionPixelNum = ScaledHeight * ScaledWidth * DetectionThreshold;

};


#endif //THUNDER_DETECTOR_VISUALDETECTOR_H
