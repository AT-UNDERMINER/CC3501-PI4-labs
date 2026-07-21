#include <opencv2/opencv.hpp>
#include <sys/time.h>

int main()
{
    // Open the video camera.
    std::string pipeline = "libcamerasrc"
        " ! video/x-raw, width=800, height=600" // camera needs to capture at a higher resolution
        " ! videoconvert"
        " ! videoscale"
        " ! video/x-raw, width=400, height=300" // can downsample the image after capturing
        " ! videoflip method=rotate-180" // remove this line if the image is upside-down
        " ! appsink drop=true max_buffers=2";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);
    if(!cap.isOpened()) {
        printf("Could not open camera.\n");
        return 1;
    }

    // Create the OpenCV window for the raw camera feed
    cv::namedWindow("Camera", cv::WINDOW_AUTOSIZE);

    // --- HSV threshold sliders (from Lab 6) ---
    cv::namedWindow("Control", cv::WINDOW_AUTOSIZE);
    int iLowH = 0;
    int iHighH = 179;

    int iLowS = 0;
    int iHighS = 255;

    int iLowV = 0;
    int iHighV = 255;

    cv::createTrackbar("LowH", "Control", &iLowH, 179); // Hue (0 - 179)
    cv::createTrackbar("HighH", "Control", &iHighH, 179);

    cv::createTrackbar("LowS", "Control", &iLowS, 255); // Saturation (0 - 255)
    cv::createTrackbar("HighS", "Control", &iHighS, 255);

    cv::createTrackbar("LowV", "Control", &iLowV, 255); // Value (0 - 255)
    cv::createTrackbar("HighV", "Control", &iHighV, 255);

    // Kernel size for morphology, stored as "half size" (0-20) and converted
    // to an odd full size below (getStructuringElement needs an odd number).
    int iKernelSize = 3;
    cv::createTrackbar("Kernel Size", "Control", &iKernelSize, 20);

    cv::namedWindow("Thresholded", cv::WINDOW_AUTOSIZE);

    cv::Mat frame, hsv_frame, thresh_frame;

    // Measure the frame rate - initialise variables
    int frame_id = 0;
    timeval start, end;
    gettimeofday(&start, NULL);

    for(;;) {
        if (!cap.read(frame)) {
            printf("Could not read a frame.\n");
            break;
        }

        // Threshold + morphology
        cv::cvtColor(frame, hsv_frame, cv::COLOR_BGR2HSV);

        cv::inRange(hsv_frame, cv::Scalar(iLowH, iLowS, iLowV),
                    cv::Scalar(iHighH, iHighS, iHighV), thresh_frame);

        int kernelSize = iKernelSize * 2 + 1;
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                     cv::Size(kernelSize, kernelSize));

        cv::morphologyEx(thresh_frame, thresh_frame, cv::MORPH_OPEN, kernel);
        cv::morphologyEx(thresh_frame, thresh_frame, cv::MORPH_CLOSE, kernel);

        cv::imshow("Thresholded", thresh_frame);

        // --- Extension: find contours, keep only the largest ---
        // findContours needs a non-const copy of the mask to work on, and it
        // modifies it in place - thresh_frame is already ours to use since we
        // just displayed it above, so this is safe.
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh_frame, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // Find the largest contour by area. Small contours are usually just
        // leftover noise that survived the morphology step; the real object
        // (the can) should be the biggest blob in the mask.
        int largestIdx = -1;
        double largestArea = 0.0;
        for (size_t i = 0; i < contours.size(); i++) {
            double area = cv::contourArea(contours[i]);
            if (area > largestArea) {
                largestArea = area;
                largestIdx = (int)i;
            }
        }

        // Draw on a clone of the original frame so we don't permanently mark
        // up the same Mat we're about to loop back and re-read next frame.
        cv::Mat display_frame = frame.clone();

        if (largestIdx >= 0) {
            // Draw the outline of just the largest contour, in white, 2px thick.
            cv::drawContours(display_frame, contours, largestIdx,
                              cv::Scalar(255, 255, 255), 2);

            // Compute centre of mass from the largest contour only (rather than
            // the whole mask), so leftover noise elsewhere in frame can't drag
            // the centroid off target.
            cv::Moments m = cv::moments(contours[largestIdx]);
            if (m.m00 > 0) {
                double cx = m.m10 / m.m00;
                double cy = m.m01 / m.m00;

                printf("Centre of mass: (%.1f, %.1f)\n", cx, cy);

                // Write the coordinates onto the image near the centroid.
                char text[64];
                snprintf(text, sizeof(text), "(%.1f, %.1f)", cx, cy);
                cv::putText(display_frame, text, cv::Point((int)cx + 10, (int)cy),
                            cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            }
        }

        cv::imshow("Camera", display_frame);
        // --- end Extension ---

        cv::waitKey(1);

        // Measure the frame rate
        frame_id++;
        if (frame_id >= 30) {
            gettimeofday(&end, NULL);
            double diff = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec)/1000000.0;
            printf("30 frames in %f seconds = %f FPS\n", diff, 30/diff);
            frame_id = 0;
            gettimeofday(&start, NULL);
        }
    }

    // Free the camera
    cap.release();
    return 0;
}
