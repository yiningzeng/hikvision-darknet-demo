#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <opencv2/opencv.hpp>
// #include <opencv2/core.hpp>
// #include <opencv2/highgui.hpp>
// #include <opencv2/imgproc.hpp>
// #include <opencv2/optflow.hpp>

// for optical flow
const double pyr_scale = 0.5;
const int levels =1;
const int winsize = 7;
const int iterations = 3;
const int poly_n = 3;
const double poly_sigma = 1.0;
const double magnitude_threshold = 1;

//for 1st stage smoothing
const cv::Size blur_filter_size = cv::Size(3, 3);
//for 2nd stage smoothing
const int R = 3;
//for final thresholding
const double thresh = 20;


static int count = 0;
const double scale = 0.3;
#ifdef DEBUG
void show(std::string name, const cv::Mat& img, double scale = 1, cv::Point pos=cv::Point(0,0), bool save = false)
{
	std::string wm_name = name + "/CV";
	cv::Mat resized;
	cv::resize(img, resized, cv::Size(), scale, scale);
	cv::Mat show;
	resized.convertTo(show, CV_8U);
	cv::namedWindow(wm_name, cv::WINDOW_FULLSCREEN);
	cv::moveWindow(wm_name, pos.x, pos.y);
	cv::imshow(wm_name, show);
	cv::waitKey(300);
	if(save) cv::imwrite(name+".png", show);
}
#else
void show(std::string name, const cv::Mat& img, double scale = 1, cv::Point pos=cv::Point(0,0), bool save = false){}
#endif


class Processor
{
public:
	bool init()//for setting parameters
	{
		return true;
	}

	void feed(const cv::Mat& frame)
	{
		cv::Mat next;
		if(frame.channels() > 1)
		{
			cv::cvtColor(frame, next, cv::COLOR_BGR2GRAY);
		}
		else
		{
			next = frame;
		}

		if(prev.empty()) 
		{
			prev = next;
			fa = cv::Mat::zeros(prev.size(), CV_32F);
			fm = cv::Mat::zeros(prev.size(), CV_32F);
			return;
		}

		cv::Mat flow;
		const int flags = 0;
		cv::calcOpticalFlowFarneback(prev, next, flow, pyr_scale, levels, winsize, iterations, poly_n, poly_sigma, flags);

		// xy to hsv
		cv::Mat flow_parts[2];//x, y components
		cv::split(flow, flow_parts);
		cv::Mat magnitude, angle;
		cv::cartToPolar(flow_parts[0], flow_parts[1], magnitude, angle, angleInDegrees);

		//show("fx" + std::to_string(count), flow_parts[0]*100, scale);
		//show("fy" + std::to_string(count), flow_parts[1]*100, scale);
		//show("mag" + std::to_string(count), magnitude*50, scale);
		//fusion
		show("mag" + std::to_string(count), magnitude*255, scale);
		cv::Mat mask = (magnitude > max(fm, magnitude_threshold));//pixels to update
		magnitude.copyTo(fm, mask);
		angle.copyTo(fa, mask);
		//show("mask", mask*255, scale);

		prev = next;
		count++;
	}

	cv::Mat post()
	{
		cv::Mat gx, gy;
		cv::polarToCart(cv::Mat(), fa, gx, gy, angleInDegrees);//empty mat means all magnitudes equal to 1

		// 1st stage smoothing
		cv::blur(gx, gx, blur_filter_size);
		cv::blur(gy, gy, blur_filter_size);
		//TODO: why not smooth the angle

		show("gx", gx*100, scale);
		show("gy", gy*100, scale);
		// 2nd stage smoothing
		cv::Mat sx = cv::Mat::zeros(gx.size(), CV_32F);
		cv::Mat sy = cv::Mat::zeros(gx.size(), CV_32F);
		cv::Mat t = cv::Mat::zeros(gx.size(), CV_32F);
		for (int i = R; i < gx.rows - R; ++i)
		{
			const float* gxi = gx.ptr<float>(i);
			const float* gyi = gy.ptr<float>(i);
			float* ti = t.ptr<float>(i);
			for (int j = R; j < gx.cols - R; ++j)
			{
				float x_sum = 0;
				float y_sum = 0;
				float w_sum = 0;
				cv::Point2f v(gxi[j], gyi[j]);

				for (int di = -R; di <= R; ++di)
				{
					const float* gxk = gx.ptr<float>(i+di);
					const float* gyl = gy.ptr<float>(i+di);
					for (int dj = -R; dj <= R; ++dj)
					{
						cv::Point2f zp(di, dj);
						float dp = v.cross(zp);
						float wp = std::max(0.f, R-dp);
						wp = wp * wp * wp;
						float dlpx = gxk[j+dj];
						float dlpy = gyl[j+dj];
						x_sum += wp * dlpx;
						y_sum += wp * dlpy;
						w_sum += wp;
					}
				}

				if(w_sum > 0)
				{
					double sx = x_sum / w_sum;
					double sy = y_sum / w_sum;
					ti[j] = 255 * (gxi[j] * sx + gyi[j] * sx) / sqrt(sx * sx + sy * sy);
				}
			}
		}

		cv::Mat t8;
		t.convertTo(t8, CV_8U);
		show("t8", t8, scale);

		cv::Mat thresholded;
		cv::threshold(t8, thresholded, thresh, 255, cv::THRESH_BINARY|cv::THRESH_OTSU); //cv::THRESH_OTSU, cv::THRESH_TRIANGLE

		//int blockSize = 35;
		//double C = 15;//threshold above mean
		//cv::adaptiveThreshold(t8, thresholded, 1.0, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, blockSize, C); //cv::ADAPTIVE_THRESH_GAUSSIAN_C

		//cv::imshow("thresholded", thresholded);
		show("thresholded", thresholded, scale);

		cv::Mat morphed;
		cv::Mat element = getStructuringElement(cv::MORPH_RECT, cv::Size(5,5));
		cv::morphologyEx(thresholded, morphed, cv::MORPH_OPEN, element);
		show("morphed", morphed, scale);

		prev = cv::Mat();
		fa = cv::Mat();
		fm = cv::Mat();

		return morphed;
	}

private:
	// for cartToPolar
	const bool angleInDegrees = true;

	cv::Mat prev;
	cv::Mat fa, fm;

	void visualize()
	{
		cv::Mat magnitude;
		cv::normalize(fm, magnitude, 0.0f, 1.0f, cv::NORM_MINMAX);
		cv::Mat angle = fa * ((1.f / 360.f) * (180.f / 255.f));

		// hsv to bgr
		cv::Mat _hsv[3], hsv, hsv8, bgr;
		_hsv[0] = angle;
		_hsv[1] = cv::Mat::ones(angle.size(), CV_32F);
		_hsv[2] = magnitude;
		cv::merge(_hsv, 3, hsv);
		hsv.convertTo(hsv8, CV_8U, 255.0);
		cv::cvtColor(hsv8, bgr, cv::COLOR_HSV2BGR);
		cv::imshow("hsv", bgr);
	}
};

Processor processor;

bool init()
{
	return true;
}

void send_input(cv::Mat frame)
{
	processor.feed(frame);
}

cv::Mat get_output()
{
	cv::Mat result = processor.post();
	return result;
}
