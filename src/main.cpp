#include "detection.h"
#include "cameras.h"
#include "calculations.h"
#include "structs.h"
#include "calibration.h"
#include "globals.h"
extern "C" {
#include "PTZF_control.h"
}

#include "opencv2/highgui/highgui.hpp"

#include <stdio.h>

/*Initialize devices*/
device left_cam {"/dev/ttyMXUSB0", 0, initializeDevice((char*)"/dev/ttyMXUSB0"), "21818297", {158, 90, 0, 0}  , {0, 0, 0, 0}};
device right_cam {"/dev/ttyMXUSB1", 1,	initializeDevice((char*)"/dev/ttyMXUSB1"), "21855432", {112, 68, 0, 0} , {0, 0, 0, 0}};

Cameras cameras;
// std::string cfg_file = "data/yolo-fe.cfg";
// std::string weights_file = "data/yolo-fe_final.weights";
std::string cfg_file = "data/yolov3-tiny-obj.cfg";
std::string weights_file = "data/yolov3-tiny-obj-3.weights";
// std::string cfg_file = "data/yolo-face.cfg";
// std::string weights_file = "data/yolo-face_final.weights";
// std::string cfg_file = "data/yolo-tiny.cfg";

Detector detector(cfg_file, weights_file);
char key_pressed;

int track_face = 1;
#include <atomic>
#include <thread>
#include <iostream>

void ReadCin(std::atomic<bool>& run)
{
	char buffer;

	while (run.load())
	{
		buffer = getch();
		if (buffer == '1') track_face = 1;
		if (buffer == '2') track_face = 2;
		if (buffer == '3') track_face = 3;
	}
}

int main() {
	std::string  names_file = "data/coco.names";
	auto obj_names = objects_names_from_file(names_file);


	if (left_cam.fd == -1 || right_cam.fd == -1) {/*exception*/return -1;}
	set_PTZF(&left_cam);
	set_PTZF(&right_cam);

	calibration_mode();

	Mat cam_img;
	std::vector<bbox> detections;
	bool left_detected, right_detected;

	std::atomic<bool> run(true);
	std::thread cinThread(ReadCin, std::ref(run));

	while (true) {

		try {
			left_detected = false;
			right_detected = false;

			left_cam.ptzf = get_position(left_cam.fd, left_cam.id);
			right_cam.ptzf = get_position(right_cam.fd, right_cam.id);

			cam_img = cameras.getFrameFromCamera(left_cam.serial_number);
			cvtColor(cam_img, cam_img, cv::COLOR_GRAY2RGB);

			detections = detect_object(cam_img, obj_names);
			for (auto &detection : detections) {
				if (detection.left > 0) {
					// printf("%d %d %d %d %d %d \n", detection.left, detection.top, detection.width, detection.height, detection.center_x, detection.center_y);

					left_detected = true;
					if (detection.type == track_face) {
						ellipse( cam_img, Point(detection.center_x, detection.center_y), Size( detection.width / 2, detection.height / 2), 0, 0, 360, Scalar( 255, 0, 255 ), 2, 8, 0 );

						left_cam.ptzf = calculatePTZF(cam_img.size().width,  cam_img.size().height, detection, left_cam);
					}
				}
			}
			imshow("Left camera", cam_img);

			cam_img = cameras.getFrameFromCamera(right_cam.serial_number);
			cvtColor(cam_img, cam_img, cv::COLOR_GRAY2RGB);
			detections = detect_object(cam_img, obj_names);

			for (auto &detection : detections) {
				if (detection.left > 0) {
					// printf("%d %d %d %d %d %d \n", detection.left, detection.top, detection.width, detection.height, detection.center_x, detection.center_y);

					right_detected = true;
					if (detection.type == track_face) {
						ellipse( cam_img, Point(detection.center_x, detection.center_y), Size( detection.width / 2, detection.height / 2), 0, 0, 360, Scalar( 255, 0, 255 ), 2, 8, 0 );

						right_cam.ptzf = calculatePTZF(cam_img.size().width,  cam_img.size().height, detection, right_cam);
					}
				}
			}
			imshow("Right camera", cam_img);

			if (!left_detected) {
				left_cam.ptzf.zoom -= 20;
			}
			if (!right_detected) {
				right_cam.ptzf.zoom -= 20;
			}
			if (left_detected && right_detected) {
				calculateCordinates();
			}

			set_PTZF(&left_cam);
			set_PTZF(&right_cam);
		}
		catch (GenICam::GenericException &e) {
			cerr << "An exception occurred.\n" << e.GetDescription() << endl;
		}

		moveWindow("Left camera", 0, 0);
		moveWindow("Right camera", 640, 0);
		waitKey(1);
	}

	run.store(false);
	cinThread.join();

	return 0;
}
