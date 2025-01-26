#ifndef THUNDER_DETECTOR_AUDIODETECTOR_H
#define THUNDER_DETECTOR_AUDIODETECTOR_H

#include "Util/Threaded.h"
#include "Util/Queue.h"
#include "SensorEvent.hpp"
#include <driver/i2s_pdm.h>



class AudioDetector : public Threaded {
public:
	/**
	 * Constructs an Audio thread. Reports detected thunder patterns to queue.
	 * @param bufferSize number of samples in a buffer
	 * @param sampleRate number of samples per second
	 * @param queue optional, output queue for receiving results
	 */
	AudioDetector(size_t bufferSize, uint16_t sampleRate, Queue<SensorEvent>* queue = nullptr);

private:
	void loop() override;
	int i2s_init(uint32_t sampling_rate);

	void detectClap(size_t startTime);
	void detectPeal();
	void detectRumble();

	const uint16_t sampleRate;
	const size_t bufferSize;

	i2s_chan_handle_t rx_chan;

	int16_t* buffer = nullptr;

	Queue<SensorEvent>* outputQueue = nullptr;


	//Clap detection
	static constexpr float EMAFactor = 0.02; //for low-pass filter determining the ambient noise value
	static constexpr int16_t ClapSpikeThreshold = 2000; //initial clap must be this amplitude above the current filtered average
	static constexpr int16_t ClapDecayThreshold = 750; //silence after clap must be this amplitude below the filtered average
	static constexpr size_t ClapDecayTimeout = 50; //[ms] decay must be achieved quickly, otherwise not a clap

	int16_t currentValue = 0; //current filtered value
	bool calibrationSample = true; //first sample is without detection, just to give EMA time to stabilize
	enum ClapDetectState {
		None, SpikeDetected, DecayDetected
	} clapState = None;

	size_t prevDecayDiff = 0;
	size_t spikeTimestamp = 0; //[ms] timestamp of last found spike, invalid if state is None


	//Converts duration of 'numSamples' to milliseconds
	constexpr size_t samplesToMs(size_t numSamples) const{
		return numSamples * 1000 / sampleRate;
	}
};


#endif //THUNDER_DETECTOR_AUDIODETECTOR_H
