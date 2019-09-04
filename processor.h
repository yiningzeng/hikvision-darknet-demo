#include <string>
#include <opencv2/core/types.hpp>

void show(std::string name, const cv::Mat& img, double scale = 1, cv::Point pos=cv::Point(0,0), bool save = false);

void send_input(cv::Mat frame);
cv::Mat get_output();
