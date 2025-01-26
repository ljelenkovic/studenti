#ifndef THUNDER_DETECTOR_SENSOREVENT_H
#define THUNDER_DETECTOR_SENSOREVENT_H

struct VideoEvent {
	uint8_t intensity; //average difference between grayscale frames with and without a sudden change, (0-255]
	//TODO - add direction to lightning detection model¸
};

//Categorisation by National Lightning Safety Institute (https://web.archive.org/web/20060717060557/http://lightningsafety.com/nlsi_info/thunder2.html)
enum class ThunderType {
	Clap,
	Peal,
	Rumble
};

struct AudioEvent {
	ThunderType type;
	//TODO - add intensity to thunder detection model
};


struct SensorEvent {
	enum class Type {
		Audio, Video
	} type;
	size_t timestamp; //[ms]

	union {
		VideoEvent video;
		AudioEvent audio;
	};
};


#endif //THUNDER_DETECTOR_SENSOREVENT_H
