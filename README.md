# CC3501 — Raspberry Pi 4 Labs

Lab work for the Linux single-board computer half of CC3501.

## Labs

| Lab | Topic |
|-----|-------|
| 6 | Embedded Linux: SD card imaging, command line, file transfer to the Pi |
| 7 | OpenCV: camera capture, HSV thresholding, contour detection |
| 8 | Networking: HTTP reverse-engineering, C++ client using libcurl |

## Hardware

- Raspberry Pi 4 + Pi camera module
- Host PC for cross-development / SSH

## Pi access

```bash
ssh pi@CC3501DT.local     
```

## Build & run

```bash
mkdir build && cd build
cmake ..
make
./<lab_executable>
```

## Reference code

- Starter app: https://github.com/bronsonp/opencv-starter-app
- Camera capture: https://github.com/bronsonp/picapture
- OpenCV docs: https://docs.opencv.org/4.x/

## Notes

- The OS lives on the SD card — reflash the image if it gets broken.
- Keep camera resolution low; image processing on the Pi is slow and resolution trades directly against latency.