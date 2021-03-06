#include <iostream>
#include <vector>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

/* overlayImage: copies a transparent 4-channel image over a solid background image.
 * Original source: http://jepsonsblog.blogspot.com.br/2012/10/overlay-transparent-image-in-opencv.html
 *
 * background: must be 3-channel BGR.
 * foreground: must be 4-channel RGBA.
 * output: the destination Mat.
 * location: offset starting point.
 */
void overlayImage(const cv::Mat &background, const cv::Mat &foreground, cv::Mat &output, cv::Point2i location)
{
    background.copyTo(output);

    // start at the row indicated by location, or at row 0 if location.y is negative.
    for (int y = std::max(location.y , 0); y < background.rows; ++y) {
        int fY = y - location.y; // because of the translation

        // we are done of we have processed all rows of the foreground image.
        if (fY >= foreground.rows)
            break;

        // start at the column indicated by location, or at column 0 if location.x is negative.
        for (int x = std::max(location.x, 0); x < background.cols; ++x) {
            int fX = x - location.x; // because of the translation.

            // we are done with this row if the column is outside of the foreground image.
            if (fX >= foreground.cols)
                break;

            // determine the opacity of the foregrond pixel, using its fourth (alpha) channel.
            double opacity = ((double)foreground.data[fY * foreground.step + fX * foreground.channels() + 3]) / 255.;

            // and now combine the background and foreground pixel, using the opacity, but only if opacity > 0.
            for (int c = 0; opacity > 0 && c < output.channels(); ++c) {
                unsigned char foregroundPx = foreground.data[fY * foreground.step + fX * foreground.channels() + c];
                unsigned char backgroundPx = background.data[y * background.step + x * background.channels() + c];
                output.data[y*output.step + output.channels()*x + c] = backgroundPx * (1.-opacity) + foregroundPx * opacity;
            }
        }
    }
}

/* getComponent: helper function that returns the color of a specific component.
 *
 * bgr_pixel: the input BGR pixel of type cv::Vec3b.
 * bgr_component: the index of the channel. 0 for Blue, 1 for Green, 2 for Red.
 * This function returns the color of a pixel as unsigned char.
 */
uchar getComponent(cv::Vec3b bgr_pixel, int bgr_component)
{
    switch (bgr_component)
    {
        case 0: // Blue
        return bgr_pixel[0];

        case 1: // Green
        return bgr_pixel[1];

        case 2: // Red
        return bgr_pixel[2];

        default:
            std::cout << "!!! getComponent: " << bgr_component << " is not a valid component" << std::endl;
        break;
  }

  return 0;
}

/* displacementMapFilter: uses the pixel values of a map to displace the pixels of the target image.
 *
 * map: the input map image (BGR).
 * target: the input foreground image (BGRA).
 * componentX: describes which color channel to use in the map image to displace the x result. Blue:0 Green:1 Red:2.
 * componentY: describes which color channel to use in the map image to displace the y result. Blue:0 Green:1 Red:2.
 * scaleX: the strength of the effect in the x plane.
 * scaleY: the strength of the effect in the y plane.
 * output: the destination Mat (BGRA).
 */
void displacementMapFilter(const cv::Mat& map, const cv::Mat& target, int componentX, int componentY, int scaleX, int scaleY, cv::Mat& output)
{
    if (componentX < 0 || componentX > 2 || componentY < 0 || componentY > 2)
    {
        std::cout << "!!! displacementMapFilter: componentX and componentY values must be in range [0,2]" << std::endl;
        return;
    }

    if (target.size().width != map.size().width || target.size().height != map.size().height || target.type() != CV_8UC4)
    {
        std::cout << "!!! displacementMapFilter: map and target need to have the same dimensions, and target type must be CV_8UC4" << std::endl;
        return;
    }

    output.create(target.rows, target.cols, target.type());

    for (int x = 0; x < output.rows; x++)
        for (int y = 0; y < output.cols; y++)
        {
            /* Formula:
            *  dstPixel[x, y] = srcPixel[x + ((componentX(x, y) - 128) * scaleX) / 256,
            *                            y + ((componentY(x, y) - 128) * scaleY) / 256)]
            */

            int dx = x + (getComponent(map.at<cv::Vec3b>(x, y), componentX) - 128) * scaleX / 256;
            if (dx < 0) dx = 0;
            if (dx >= output.rows) dx = output.rows;

            int dy = y + (getComponent(map.at<cv::Vec3b>(x, y), componentY) - 128) * scaleY / 256;
            if (dy < 0) dy = 0;
            if (dy >= output.cols) dy = output.cols;

            output.at<cv::Vec4b>(x, y) = target.at<cv::Vec4b>(dx, dy);
        }
}


int main(int argc, char* argv[])
{
    // Load input map (colored, 3-channel, BGR)
    cv::Mat map = cv::imread("map.jpg");
    if (map.empty())
    {
        std::cout << "!!! Failed imread() #1" << std::endl;
        return -1;
    }
    std::cout << "map size: " << map.cols << "x" << map.rows << " channels:" << map.channels() << " type:" << map.type() << std::endl;

    // Load input target (colored, 4-channel, BGRA)
    cv::Mat target = cv::imread("target.png", -1);
    if (target.empty())
    {
        std::cout << "!!! Failed imread() #2" << std::endl;
        return -1;
    }
    std::cout << "target size: " << target.cols << "x" << target.rows << "channels: " << target.channels() << " type: " << target.type() << std::endl;

    if (target.channels() != 4)
    {
        std::cout << "!!! A PNG image with transparent layer is required" << std::endl;
        return -1;
    }

    if (target.size().width > map.size().width || target.size().height > map.size().height)
    {
        std::cout << "!!! Target needs to have smaller dimensions than map" << std::endl;
        return -1;
    }

    /* Display the map as movie clip */

    int offset_x = 0;

    while (1)
    {
        // Crop the map (which is larger) to the size of the target image
        cv::Rect roi = cv::Rect(offset_x, 0, target.size().width, target.size().height);
        cv::Mat cropped_map = map(roi);

        // Execute the Displacement Map Filter
        cv::Mat output;
        displacementMapFilter(cropped_map, target, 2, 2, 20, 20, output);

        // Display the results on the screen
        cv::Mat frame;
        overlayImage(cropped_map, output, frame, cv::Point(0,0));
        cv::imshow("OpenCV - Displacement Map Filter", frame);

        // Detect if ESC was pressed and quit. Frames are displayed every 33ms.
        char key = cv::waitKey(33);
        if (key == 27)
            break;

        // You can increase the value of offset_x to play the animation faster
        offset_x += 3;
        if ((map.size().width - target.size().width) <= offset_x)
            break;
    }

    return 0;
}

