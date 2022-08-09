/*
* Copyright 2016 Nu-book Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "ReadBarcode.h"
#include "TextUtfEncoding.h"
#include "GTIN.h"
//#include "dmtx.h"

#include <cctype>
#include <chrono>
#include <clocale>
#include <cstring>
#include <iostream>
//std:c++latest
#include <memory>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
namespace fs = std::filesystem;
//#include <experimental/filesystem>
//namespace fs = std::experimental::filesystem;
using namespace ZXing;
using namespace TextUtfEncoding;

#define color_green cv::Scalar(0, 255, 0)
#define color_blue cv::Scalar(255, 0, 0)
#define color_red cv::Scalar(0, 0, 255)
#define color_purple cv::Scalar(255, 0, 255)
#define color_white cv::Scalar(255, 255, 255)

cv::Mat norm_brightness_c(cv::Mat input);
cv::Mat gammaCorrectionAndContarast(cv::Mat image, float gamma, float contrast);
cv::Mat apply_CLAHE(cv::Mat img);
int count_file = 0;
static void PrintUsage(const char* exePath)
{
	std::cout << "Usage: " << exePath << " [-fast] [-norotate] [-format <FORMAT[,...]>] [-pngout <png out path>] [-ispure] [-1] <png image path>...\n"
			  << "    -fast      Skip some lines/pixels during detection (faster)\n"
			  << "    -norotate  Don't try rotated image during detection (faster)\n"
			  << "    -format    Only detect given format(s) (faster)\n"
			  << "    -ispure    Assume the image contains only a 'pure'/perfect code (faster)\n"
			  << "    -1         Print only file name, text and status on one line per file\n"
			  << "    -escape    Escape non-graphical characters in angle brackets (ignored for -1 option, which always escapes)\n"
			  << "    -pngout    Write a copy of the input image with barcodes outlined by a red line\n"
			  << "\n"
			  << "Supported formats are:\n";
	for (auto f : BarcodeFormats::all()) {
		std::cout << "    " << ToString(f) << "\n";
	}
	std::cout << "Formats can be lowercase, with or without '-', separated by ',' and/or '|'\n";
}

static bool ParseOptions(int argc, char* argv[], DecodeHints& hints, bool& oneLine, bool& angleEscape,
						 std::vector<std::string>& filePaths, std::string& outPath)
{
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-fast") == 0) {
			hints.setTryHarder(false);
		}
		else if (strcmp(argv[i], "-norotate") == 0) {
			hints.setTryRotate(false);
		}
		else if (strcmp(argv[i], "-ispure") == 0) {
			hints.setIsPure(true);
			hints.setBinarizer(Binarizer::FixedThreshold);
		}
		/*else if (strcmp(argv[i], "-format") == 0) {
			if (++i == argc)
				return false;
			try {
				hints.setFormats(BarcodeFormatsFromString(argv[i]));
			} catch (const std::exception& e) {
				std::cerr << e.what() << "\n";
				return false;
			}
		}*/
		else if (strcmp(argv[i], "-1") == 0) {
			oneLine = true;
		}
		else if (strcmp(argv[i], "-escape") == 0) {
			angleEscape = true;
		}
		/*
		else if (strcmp(argv[i], "-pngout") == 0) {
			if (++i == argc)
				return false;
			outPath = argv[i];
		}*/
		else {
			filePaths.push_back(argv[i]);
		}
	}
	filePaths.push_back("C:/Users/yarrr/Pictures/1d_zalupa.jpg");
	count_file = 1;
	//std::string path = "C:\\Users\\yarrr\\Pictures\\test_with_plenka\\without_filter";
	//for (auto& p : fs::directory_iterator(path)) {
	//	std::cout << p.path().u8string() << std::endl;

	//	filePaths.push_back(p.path().u8string());
	//	count_file++;
	//}
	return !filePaths.empty();
}

std::ostream& operator<<(std::ostream& os, const Position& points) {
	for (const auto& p : points)
		os << p.x << "x" << p.y << " ";
	return os;
}

void drawLine(const ImageView& image, PointI a, PointI b)
{
	int steps = maxAbsComponent(b - a);
	PointF dir = bresenhamDirection(PointF(b - a));
	for (int i = 0; i < steps; ++i) {
		auto p = centered(a + i * dir);
		*((uint32_t*)image.data(p.x, p.y)) = 0xff0000ff;
	}
}

void drawRect(const ImageView& image, const Position& pos)
{
	for(int i=0;i<4;++i)
		drawLine(image, pos[i], pos[(i+1)%4]);
}

int main(int argc, char* argv[])
{
	DecodeHints hints;
	hints.setFormats(BarcodeFormatsFromString("DataMatrix Code128 Code93 Code39 DataBar UPC-A UPC-E EAN-8 Aztec EAN-13 Codabar PDF417 ITF DataBarExpanded"));
	cv::Mat img_roi;
	cv::Mat img_fin;
	std::vector<std::string> filePaths;
	std::string outPath;
	bool oneLine = false;
	bool angleEscape = false;
	int ret = 0;

	if (!ParseOptions(argc, argv, hints, oneLine, angleEscape, filePaths, outPath)) {
		PrintUsage(argv[0]);
		return -1;
	}

	hints.setEanAddOnSymbol(EanAddOnSymbol::Read);

	if (oneLine)
		angleEscape = true;

	if (angleEscape)
		std::setlocale(LC_CTYPE, "en_US.UTF-8"); // Needed so `std::iswgraph()` in `ToUtf8(angleEscape)` does not 'swallow' all printable non-ascii utf8 chars
	//cv::VideoCapture cap(2);

	/*
	for (const auto& filePath : filePaths) {
		int width, height, channels;
	*/

	std::string windowName = "Orginal"; //Name of the window
	cv::namedWindow(windowName); // Create a window

	std::string windowNameF = "Filter"; //Name of the window
	cv::namedWindow(windowNameF); // Create a window

	std::string windowZxing = "Zxingoutput"; //Name of the window
	cv::namedWindow(windowZxing); // Create a window
	int count = 0;

	int count_true_code = 0;
	while(true){
		if (count == count_file)break;
		std::cout << count << " -- " << count_true_code << "\n";
		/*
		std::unique_ptr<stbi_uc, void(*)(void*)> buffer(stbi_load(filePath.c_str(), &width, &height, &channels, 4), stbi_image_free);
		if (buffer == nullptr) {
			std::cerr << "Failed to read image: " << filePath << "\n";
			return -1;
		}
		
		ImageView image{buffer.get(), width, height, ImageFormat::RGBX};
		*/

		cv::Mat image;
		//cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
		//cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
		//cap >> image;
		image = cv::imread(filePaths[count]);
		//std::cout << image.cols << "   " << image.rows;
		//image = image(cv::Range(0, image.cols), cv::Range(1000, 1500));

		if (image.empty()) {
			std::cout <<  "\nName  " << filePaths[count] << " Out";
			std::cout << "\nNameR C://Users//yarrr//Pictures//test_with_plenka//shot_roi_2897_0.jpg\n";
			count++;
			continue;





		}
		char c = (char)cv::waitKey(25);
		if (c == 27)
			break;

		count++;
		//std::cout << "todo \n";
		auto t1_ = std::chrono::high_resolution_clock::now();
		//image = norm_brightness_c(image);
		//image = gammaCorrectionAndContarast(image,0.9,100);
		img_roi = image;
		auto t2_ = std::chrono::high_resolution_clock::now();
		int milliseconds = std::chrono::duration_cast<std::chrono::microseconds>(t2_ - t1_).count();
		int col = 0;
		auto t3_ = std::chrono::high_resolution_clock::now();

		//std::cout << "todo \n";
		const ZXing::Result result = ReadBarcode({ img_roi.data, img_roi.cols, img_roi.rows, ImageFormat::BGR }, hints);
		std::string value_temp = TextUtfEncoding::ToUtf8(result.text());

		bool stop = false;
		if (value_temp != "") {

			// if(value == value_temp and value!="") std::cout << "NOT OKEY\n";
			value_temp;
			// std::cout << "узрел";
			stop = true;
			std::cout << "Text:     \"" << ToUtf8(result.text(), angleEscape) << "\"\n"
				<< "Format:   " << ToString(result.format()) << "\n"

				<< "ECCodeWords: " << result.Deviations.numECCodeWords << "\n"
				<< "sizeErrorLocations: " << result.Deviations.sizeErrorLocations << "\n"
				<< "sigmaTildeAtZero: " << result.Deviations.sigmaTildeAtZero << "\n"
				<< "degree: " << result.Deviations.degree << "\n"

				<< "Position: " << result.position() << "\n"
				<< "Rotation: " << result.orientation() << " deg\n"
				<< "Error:    " << ToString(result.status()) << "\n";
		}
		//else {
		//	// std::cout << "LibDTMX_wait\n";
		//	DmtxImage* img = dmtxImageCreate(img_roi.data, img_roi.cols, img_roi.rows, DmtxPack24bppBGR);
		//	DmtxDecode* dec = dmtxDecodeCreate(img, 1);
		//	DmtxTime time = dmtxTimeAdd(dmtxTimeNow(), 25);
		//	DmtxRegion* reg = dmtxRegionFindNext(dec, &time);
		//	if (reg != NULL) {
		//		DmtxMessage* msg = dmtxDecodeMatrixRegion(dec, reg, DmtxUndefined);
		//		if (msg != NULL) {
		//			// std::string value_libdmtx(reinterpret_cast<char*>(msg->output));
		//			value_temp = std::string(msg->output, msg->output + msg->outputIdx);
		//			// if(value == value_temp and value!="") std::cout << "NOT OKEY\n";
		//			// std::cout << "LibDTMX_work\n";
		//			stop = true;
		//			std::cout << "DMTX decoder\n"
		//				<< "Text: " << value_temp
		//				<< "\n";
		//		}
		//	}
		//}
		//std::cout << "ну что народ" << std::endl;
		auto t4_ = std::chrono::high_resolution_clock::now();
		milliseconds = std::chrono::duration_cast<std::chrono::microseconds>(t4_ - t3_).count();
		//std::cout << "\n RB = " << milliseconds << " ms\n\n";
		//for (ZXing::Result result : results) {
		ZXing::Position pos = result.position();
		//std::cout << "todo \n";
		cv::line(img_roi, cv::Point(pos.topLeft().x, pos.topLeft().y), cv::Point(pos.topRight().x, pos.topRight().y), color_blue, 2);
		cv::line(img_roi, cv::Point(pos.topRight().x, pos.topRight().y), cv::Point(pos.bottomRight().x, pos.bottomRight().y), color_blue, 2);
		cv::line(img_roi, cv::Point(pos.bottomRight().x, pos.bottomRight().y), cv::Point(pos.bottomLeft().x, pos.bottomLeft().y), color_blue, 2);
		cv::line(img_roi, cv::Point(pos.bottomLeft().x, pos.bottomLeft().y), cv::Point(pos.topLeft().x, pos.topLeft().y), color_blue, 2);
		/*
		cv::line(image, cv::Point(pos.topLeft().x, pos.topLeft().y), cv::Point(pos.topRight().x, pos.topRight().y), color_blue, 2);
		cv::line(image, cv::Point(pos.topRight().x, pos.topRight().y), cv::Point(pos.bottomRight().x, pos.bottomRight().y), color_blue, 2);
		cv::line(image, cv::Point(pos.bottomRight().x, pos.bottomRight().y), cv::Point(pos.bottomLeft().x, pos.bottomLeft().y), color_blue, 2);
		cv::line(image, cv::Point(pos.bottomLeft().x, pos.bottomLeft().y), cv::Point(pos.topLeft().x, pos.topLeft().y), color_blue, 2);
		*/
		//}
	//uint8_t* LumData = ReadBarcodeLum({ img_roi.data, img_roi.cols, img_roi.rows, ImageFormat::BGR }, hints);

	//cv::Mat img_lum = cv::Mat{ img_roi.rows , img_roi.cols ,CV_8UC1 ,LumData };
	//cv::imshow(windowZxing, img_lum);

		//cv::Mat image_show;
		//cv::resize(img_roi, image_show, cv::Size(), 0.25, 0.25);
		//cv::imshow(windowName, image);


		//std::cout << "errorMagnitudes: \n";

		cv::imshow(windowNameF, img_roi);
		if (stop) {
			//cv::waitKey(0);
			count_true_code++;
		}
		
	}
	cv::waitKey(0);

	return ret;
}
using namespace cv;
cv::Mat apply_CLAHE(cv::Mat img)
{
	cv::Ptr<cv::CLAHE> clahe;
	clahe = cv::createCLAHE();
	clahe->setClipLimit(4);
	cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);
	clahe->apply(img, img);
	//cv::adaptiveThreshold(img, img,255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 53, 9);
	cv::cvtColor(img, img, cv::COLOR_GRAY2BGR);
	return img;
}

cv::Mat gammaCorrectionAndContarast(cv::Mat image, float gamma, float contrast)
{
	cv::Mat res = image.clone();
	double alpha_ = 131 * (contrast + 127) / (127 * (131 - contrast));
	double gamma_ = 127 * (1 - alpha_);
	cv::addWeighted(image, alpha_, image, 0, gamma_, res);

	cv::Mat lookUpTable(1, 256, CV_8U);
	uchar* p = lookUpTable.ptr();
	gamma = 1 / gamma;
	for (int i = 0; i < 256; ++i)
		p[i] = cv::saturate_cast<uchar>(pow(i / 255.0, gamma) * 255.0);
	cv::Mat corrected = image.clone();
	cv::LUT(res, lookUpTable, corrected);

	return corrected;
}
cv::Mat norm_brightness_c(cv::Mat input) {
	cv::Mat dst = cv::Mat();
	cv::cvtColor(input, dst, cv::COLOR_BGR2GRAY);
	cv::Mat at = cv::Mat();
	cv::adaptiveThreshold(dst, at, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY, 21, 13);
	cv::Scalar bgcolor = cv::mean(input, at);
	cv::Scalar textcolor = cv::mean(input, 255 - at);
	cv::Mat result = input;
	//uint8_t* MatData = input.data;

	int Size = input.cols * input.rows;
	for (int i = 0; i < Size; i++) {
		if (*(at.data + i) == 255) {
			result.data[i * 3] = bgcolor[0];
			result.data[i * 3 + 1] = bgcolor[1];
			result.data[i * 3 + 2] = bgcolor[2];
		}
		if (*(at.data + i) == 0) {
			result.data[i * 3] = textcolor[0];
			result.data[i * 3 + 1] = textcolor[1];
			result.data[i * 3 + 2] = textcolor[2];
		}
	}
	return result;
}
