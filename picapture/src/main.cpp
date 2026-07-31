// Lab 7: real time colour object detection with OpenCV.
//
// Captures live frames from the Raspberry Pi camera, thresholds them in HSV
// colour space, cleans up the result with morphological open and close, then
// reports the centre of mass of the largest detected object.
//
// All detection parameters are adjustable at run time using the sliders in the
// "Control" window. Press Escape or q to quit.

#include <opencv2/opencv.hpp>
#include <sys/time.h>

// The camera captures at a higher resolution than we process. Downsampling
// after capture keeps the full field of view while making each frame much
// cheaper to process on the Pi.
const int CAPTURE_WIDTH = 800;
const int CAPTURE_HEIGHT = 600;
const int PROCESS_WIDTH = 400;
const int PROCESS_HEIGHT = 300;

// Slider limits. Hue in OpenCV runs 0 to 179, the other channels 0 to 255.
const int HUE_MAX = 179;
const int CHANNEL_MAX = 255;
const int KERNEL_HALF_MAX = 20;
const int MIN_AREA_MAX = 5000;

// Number of frames to time before reporting the frame rate.
const int FRAME_RATE_INTERVAL = 30;

// Appearance of the detection overlay.
const cv::Scalar OVERLAY_COLOUR(255, 255, 255);
const int OUTLINE_THICKNESS = 2;
const double LABEL_SCALE = 0.5;
const int LABEL_OFFSET = 10;

const std::string CAMERA_WINDOW = "Camera";
const std::string THRESHOLD_WINDOW = "Thresholded";
const std::string CONTROL_WINDOW = "Control";

// Detection parameters, all controlled by the sliders.
struct DetectionParams {
    int low_hue = 0;
    int high_hue = HUE_MAX;
    int low_saturation = 0;
    int high_saturation = CHANNEL_MAX;
    int low_value = 0;
    int high_value = CHANNEL_MAX;

    // Half the width of the structuring element. getStructuringElement needs an
    // odd size, so the full size used is 2 * kernel_half_size + 1.
    int kernel_half_size = 3;

    // Contours smaller than this are treated as leftover noise and ignored.
    int min_area = 100;
};

// Times how long each block of frames takes so the frame rate can be reported.
struct FrameRateMonitor {
    timeval start;
    int frame_count = 0;

    void begin()
    {
        frame_count = 0;
        gettimeofday(&start, NULL);
    }

    // Counts one frame, printing the average rate once a full block is done.
    void tick()
    {
        frame_count++;
        if (frame_count < FRAME_RATE_INTERVAL) {
            return;
        }

        timeval end;
        gettimeofday(&end, NULL);
        double seconds = (end.tv_sec - start.tv_sec) +
                         (end.tv_usec - start.tv_usec) / 1000000.0;
        printf("%d frames in %.3f seconds = %.1f FPS\n",
               frame_count, seconds, frame_count / seconds);

        begin();
    }
};

// Builds the gstreamer pipeline used to read frames from the Pi camera.
std::string build_pipeline()
{
    return "libcamerasrc"
           " ! video/x-raw, width=" + std::to_string(CAPTURE_WIDTH) +
           ", height=" + std::to_string(CAPTURE_HEIGHT) +
           " ! videoconvert"
           " ! videoscale"
           " ! video/x-raw, width=" + std::to_string(PROCESS_WIDTH) +
           ", height=" + std::to_string(PROCESS_HEIGHT) +
           " ! videoflip method=rotate-180" // remove this line if the image is upside-down
           " ! appsink drop=true max_buffers=2";
}

// Creates the window of sliders used to tune the detection while it runs.
void create_control_window(DetectionParams &params)
{
    cv::namedWindow(CONTROL_WINDOW, cv::WINDOW_AUTOSIZE);

    cv::createTrackbar("LowH", CONTROL_WINDOW, &params.low_hue, HUE_MAX);
    cv::createTrackbar("HighH", CONTROL_WINDOW, &params.high_hue, HUE_MAX);

    cv::createTrackbar("LowS", CONTROL_WINDOW, &params.low_saturation, CHANNEL_MAX);
    cv::createTrackbar("HighS", CONTROL_WINDOW, &params.high_saturation, CHANNEL_MAX);

    cv::createTrackbar("LowV", CONTROL_WINDOW, &params.low_value, CHANNEL_MAX);
    cv::createTrackbar("HighV", CONTROL_WINDOW, &params.high_value, CHANNEL_MAX);

    cv::createTrackbar("Kernel Size", CONTROL_WINDOW, &params.kernel_half_size, KERNEL_HALF_MAX);
    cv::createTrackbar("Min Area", CONTROL_WINDOW, &params.min_area, MIN_AREA_MAX);
}

// Converts the frame to HSV, thresholds it against the slider values, then
// applies morphological open and close to tidy up the result.
//
// The HSV buffer is passed in rather than declared locally so that it is
// reused between frames instead of being reallocated every time.
void threshold_image(const cv::Mat &frame, const DetectionParams &params,
                     cv::Mat &hsv, cv::Mat &mask)
{
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    cv::inRange(hsv,
                cv::Scalar(params.low_hue, params.low_saturation, params.low_value),
                cv::Scalar(params.high_hue, params.high_saturation, params.high_value),
                mask);

    int kernel_size = params.kernel_half_size * 2 + 1;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                               cv::Size(kernel_size, kernel_size));

    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);  // removes speckles in the background
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel); // fills small holes in the object
}

// Finds the objects in the thresholded mask. Contours smaller than min_area are
// discarded as noise, and the index of the largest object that remains is
// returned, or -1 if nothing was detected.
int find_objects(const cv::Mat &mask, std::vector<std::vector<cv::Point>> &objects,
                 double min_area)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    objects.clear();
    int largest_index = -1;
    double largest_area = 0;

    for (const auto &contour : contours) {
        double area = cv::contourArea(contour);
        if (area < min_area) {
            continue;
        }

        if (area > largest_area) {
            largest_area = area;
            largest_index = static_cast<int>(objects.size());
        }
        objects.push_back(contour);
    }

    return largest_index;
}

// Calculates the centre of mass of a contour from its image moments. Returns
// false if the contour encloses no area.
bool centre_of_mass(const std::vector<cv::Point> &contour, cv::Point2d &centre)
{
    cv::Moments moments = cv::moments(contour);
    if (moments.m00 <= 0) {
        return false;
    }

    centre.x = moments.m10 / moments.m00;
    centre.y = moments.m01 / moments.m00;
    return true;
}

// Draws an outline around each detected object.
void draw_outlines(cv::Mat &image, const std::vector<std::vector<cv::Point>> &objects)
{
    cv::drawContours(image, objects, -1, OVERLAY_COLOUR, OUTLINE_THICKNESS);
}

// Writes the centre of mass coordinates onto the image, next to that point.
void draw_centre_label(cv::Mat &image, const cv::Point2d &centre)
{
    char label[32];
    snprintf(label, sizeof(label), "%.1f, %.1f", centre.x, centre.y);

    cv::Point position(static_cast<int>(centre.x) + LABEL_OFFSET, static_cast<int>(centre.y));
    cv::putText(image, label, position, cv::FONT_HERSHEY_SIMPLEX, LABEL_SCALE, OVERLAY_COLOUR);
}

int main()
{
    cv::VideoCapture camera(build_pipeline(), cv::CAP_GSTREAMER);
    if (!camera.isOpened()) {
        printf("Could not open camera.\n");
        return 1;
    }

    DetectionParams params;
    cv::namedWindow(CAMERA_WINDOW, cv::WINDOW_AUTOSIZE);
    cv::namedWindow(THRESHOLD_WINDOW, cv::WINDOW_AUTOSIZE);
    create_control_window(params);

    // Declared outside the loop so the buffers are reused for every frame.
    cv::Mat frame, hsv_frame, thresh_frame, display_frame;
    std::vector<std::vector<cv::Point>> objects;

    FrameRateMonitor frame_rate;
    frame_rate.begin();

    for (;;) {
        if (!camera.read(frame)) {
            printf("Could not read a frame.\n");
            break;
        }

        threshold_image(frame, params, hsv_frame, thresh_frame);
        cv::imshow(THRESHOLD_WINDOW, thresh_frame);

        int largest = find_objects(thresh_frame, objects, params.min_area);

        // Annotate a copy so that the captured frame itself stays unmarked.
        frame.copyTo(display_frame);
        draw_outlines(display_frame, objects);

        // The centre of mass is taken from the largest object rather than the
        // whole mask, so that any noise elsewhere in the frame cannot pull the
        // reported position away from the object being tracked.
        cv::Point2d centre;
        if (largest >= 0 && centre_of_mass(objects[largest], centre)) {
            printf("Centre of mass: (%.1f, %.1f)\n", centre.x, centre.y);
            draw_centre_label(display_frame, centre);
        }

        cv::imshow(CAMERA_WINDOW, display_frame);
        frame_rate.tick();

        // The upper bits of the return value can carry modifier keys, so mask
        // them off before comparing against the key we are looking for.
        int key = cv::waitKey(1) & 0xFF;
        if (key == 27 || key == 'q') { // Escape or q
            break;
        }
    }

    camera.release();
    cv::destroyAllWindows();
    return 0;
}
