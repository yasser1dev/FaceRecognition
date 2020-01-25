#include <dlib/opencv.h>
#include <opencv2/highgui/highgui.hpp>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <iostream>
#include <fstream>
#include <dlib/dnn.h>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using namespace dlib;
using namespace std;

// ----------------------------------------------------------------------------------------

// The next bit of code defines a ResNet network.  It's basically copied
// and pasted from the dnn_imagenet_ex.cpp example, except we replaced the loss
// layer with loss_metric and made the network somewhat smaller.  Go read the introductory
// dlib DNN examples to learn what all this stuff means.
//
// Also, the dnn_metric_learning_on_images_ex.cpp example shows how to train this network.
// The dlib_face_recognition_resnet_model_v1 model used by this example was trained using
// essentially the code shown in dnn_metric_learning_on_images_ex.cpp except the
// mini-batches were made larger (35x15 instead of 5x5), the iterations without progress
// was set to 10000, and the training dataset consisted of about 3 million images instead of
// 55.  Also, the input layer was locked to images of size 150.
template <template <int, template<typename>class, int, typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual = add_prev1<block<N, BN, 1, tag1<SUBNET>>>;

template <template <int, template<typename>class, int, typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual_down = add_prev2<avg_pool<2, 2, 2, 2, skip1<tag2<block<N, BN, 2, tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET>
using block = BN<con<N, 3, 3, 1, 1, relu<BN<con<N, 3, 3, stride, stride, SUBNET>>>>>;

template <int N, typename SUBNET> using ares = relu<residual<block, N, affine, SUBNET>>;
template <int N, typename SUBNET> using ares_down = relu<residual_down<block, N, affine, SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256, SUBNET>;
template <typename SUBNET> using alevel1 = ares<256, ares<256, ares_down<256, SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128, ares<128, ares_down<128, SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64, ares<64, ares<64, ares_down<64, SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32, ares<32, ares<32, SUBNET>>>;

using anet_type = loss_metric<fc_no_bias<128, avg_pool_everything<
	alevel0<
	alevel1<
	alevel2<
	alevel3<
	alevel4<
	max_pool<3, 3, 2, 2, relu<affine<con<32, 7, 7, 2, 2,
	input_rgb_image_sized<150>
	>>>>>>>>>>>>;

int main(int argc, char** argv)
{
	try
	{
		if (argc != 2) {
			cout << "Run this program by invoking it like this: " << endl;
			cout << "   program_name.exe config.txt" << endl;
			cout << endl;
			return 1;
		}
		std::ifstream input(argv[1]);
		string DB_url, login, password, DB_name, camera_url,landmark_model;
		for (std::string line; getline(input, line); )
		{
			std::vector<string> data;

			std::string delimiter = ";";

			size_t pos = 0;
			std::string token;
			while ((pos = line.find(delimiter)) != std::string::npos) {
				token = line.substr(0, pos);
				//std::cout << token << std::endl;
				data.push_back(token);
				line.erase(0, pos + delimiter.length());
			}
			DB_url= data[0];
			login = data[1];
			password = data[2];
			DB_name = data[3];
			camera_url = data[4];
			landmark_model = data[5];
		}
	



		sql::Driver *driver;
		sql::Connection *con;
		sql::Statement *stmt;
		sql::ResultSet *res;

		// Create a connection 
		driver = get_driver_instance();
		con = driver->connect(DB_url,login,password);

		// Connect to the MySQL test database 
		con->setSchema(DB_name);

		if (con) {
			cout << "Connected to the DataBase!" << endl;
		}

		stmt = con->createStatement();

		// Check if the DB is empty or not.
		bool isEmpty;
		res = stmt->executeQuery("SELECT count(*) FROM facedata");
		while (res->next()) {
			cout << res->getInt(1) << endl;
			if (res->getInt(1) == 0) {
				isEmpty = true;
			}
			else {
				isEmpty = false;
			}
		}

		// Connect to the streaming camera.
		int codeCam;
		codeCam = stoi(camera_url);
		cv::VideoCapture cap(codeCam);


		if (!cap.isOpened())
		{
			cerr << "Unable to connect to camera" << endl;
			return 1;
		}

		image_window win;

		// Load face detection and pose estimation models.
		frontal_face_detector detector = get_frontal_face_detector();
		shape_predictor pose_model;
		deserialize(landmark_model) >> pose_model;

		// Load the DNN responsible for face recognition.
		anet_type net;
		deserialize("dlib_face_recognition_resnet_model_v1.dat") >> net;

		// Grab and process the frames until the window is closed by the user.
		while (!win.is_closed()) {

			// Grab a frame
			cv::Mat frame;
			if (!cap.read(frame)) {
				cout << "!cap.read(frame)" << endl;
				break;
			}

			// Turn OpenCV's Mat into something dlib can deal with.  Note that this just
			// wraps the Mat object, it doesn't copy anything.  So cimg is only valid as
			// long as temp is valid.  Also don't do anything to temp that would cause it
			// to reallocate the memory which stores the image as that will make cimg
			// contain dangling pointers.  This basically means you shouldn't modify temp
			// while using cimg.
			cv_image<bgr_pixel> cimg(frame);

			// Detect faces 
			std::vector<rectangle> faces = detector(cimg);

			// Where we save the 128D for each face detected in each frame.
			std::vector<string> faces_128D;
			std::vector<string> real_names;
			std::vector<matrix<float, 0, 1>> faces_detected_128D;
			std::vector<matrix<float, 0, 1>> faces_in_DB;
			matrix<rgb_pixel> face_chip;

			if (faces.size() != 0) { 
				cout << faces.size() << " FACE(s) DETECTED!" << endl;

				for (int i = 0; i < faces.size(); i++) {
					
					full_object_detection shape = pose_model(cimg, faces[i]);
					extract_image_chip(cimg, get_face_chip_details(shape, 150, 0.25), face_chip);

					matrix<float, 0, 1> face_descriptor = net(face_chip);
					faces_detected_128D.push_back(face_descriptor);

					string str = "";

					for (int i = 0; i < 128; i++) {
						str += to_string(face_descriptor(i, 0)) + ";";
					}

					faces_128D.push_back(str);

				}

				if (isEmpty) { // if the DB is empty we directly insert the detected faces into it.
					cout << "empty DB!" << endl;
					for (int i = 0; i < faces_128D.size(); i++) {
						stmt->executeUpdate("INSERT INTO facedata (face_data) values('" + faces_128D[i] + "')");
						cout << "New face added to the DB!" << endl;
					}
					isEmpty = false;
				}
				else { // if the DB is not empty, we extract the 128D in it and we prepare them for the comparison
					matrix<float, 128, 1> current_128D_inDB;
					string dets, token;
					string delimiter = ";";

					res = stmt->executeQuery("SELECT face_data,name FROM facedata");
					while (res->next()) {
						dets = res->getString("face_data");
						int k = 0;
						size_t pos = 0;

						while ((pos = dets.find(delimiter)) != string::npos) {
							token = dets.substr(0, pos);
							current_128D_inDB(k, 0) = stof(token);
							dets.erase(0, pos + delimiter.length());
							k++;
						}

						faces_in_DB.push_back(current_128D_inDB);
						real_names.push_back(res->getString("name"));
					}

					// Compare
					bool pExist;
					win.clear_overlay();
					win.set_image(cimg);
					std::clock_t start;
					double duration;
					start = std::clock();
					for (int i = 0; i < faces_detected_128D.size(); i++) {
						for (int j = 0; j < faces_in_DB.size(); j++) {
							if (length(faces_detected_128D[i] - faces_in_DB[j])<0.6) {
								// Display it all on the screen
								cout << "lenght is :" << length(faces_detected_128D[i] - faces_in_DB[j]) << endl;
								float p = (length(faces_detected_128D[i] - faces_in_DB[j]) / 0.60) * 100;
								win.add_overlay(image_window::overlay_rect(faces[i], rgb_pixel(0, 0, 255), to_string(p) + "%" + real_names[j]));

								cout << "Welcome :" << real_names[j] <<endl;
								pExist = true;
								break;
							}
							else {
								//cout << "lenght with " << real_names[j] << " is :" <<length(faces_detected_128D[i] - faces_in_DB[j]) << endl;
								pExist = false;
							}
						}

						if (!pExist) {
							// Display it all on the screen

							win.add_overlay(faces);
							string s = "UNKNOWN";
							cout << s << endl;
						}
					}
					duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;
					std::cout << "Operation took " << duration << "seconds" << std::endl;
				}

			}
			else {
				cout << "No face is detected atm!" << endl;

				// Display it all on the screen
				win.clear_overlay();
				win.set_image(cimg);
			}
		}
	}
	catch (serialization_error& e)
	{
		cout << "You need dlib's default face landmarking model file to run this example." << endl;
		cout << "You can get it from the following URL: " << endl;
		cout << "   http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2" << endl;
		cout << endl << e.what() << endl;
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}
}

