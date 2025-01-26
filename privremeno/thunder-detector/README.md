# Linkovi, ideje

https://www.nssl.noaa.gov/education/svrwx101/lightning/detection/

https://ghrc.nsstc.nasa.gov/lightning/lightning-detections.html

https://web.archive.org/web/20060717060557/http://lightningsafety.com/nlsi_info/thunder2.html

https://arxiv.org/pdf/2204.08026

https://www.instructables.com/Thunderstorm-Lightning-Detector-and-Camera-Trigger/


Audio:
3 podvrste zvučnih profila (možda samo jedan uočiti)
Pljesak 
   konstante clap thresholda, duljina i threshold decaya iza clappa
   EMA filter, uoči povećanje, uoči decay

Peal - više clapova, s izmjenama u amplitudi, možda modelirati kao agregat pljeskova?

Rumble - FFT i povećanje amplitude na frekvenciji 63 Hz kroz dulje vrijeme

Video:
OpenCV grayscaleanje
gledaj naglu razliku u kontrastu
pattern recognition samog grananja munje je težak

## Performanse

frame get: 45ms, detection: 9ms, total: 54ms, fps: 18.52
