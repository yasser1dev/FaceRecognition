#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>
#include <dlib/string.h>
#include <dlib/image_io.h>
#include <iostream>

#include <dlib/dnn.h>

#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <fstream>

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

// ----------------------------------------------------------------------------------------


int main(int argc, char** argv)
{
	try
	{

		if (argc != 2) {
			cout << "Run this example by invoking it like this: " << endl;
			cout << "   add_face_todb data.txt" << endl;
			cout << endl;
			return 1;
		}
		std::ifstream input("config.txt");
		string DB_url, login, password, DB_name,landmark_model;
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
			DB_url = data[0];
			login = data[1];
			password = data[2];
			DB_name = data[3];
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
		std::ifstream input1(argv[1]);

		for (std::string line; getline(input1, line); )
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

			// Load the provided img and the full name.
			matrix<rgb_pixel> img;
			string path = data[0];
			load_image(img, path);

			string last_name = data[2];
			string first_name = data[1];
			string full_name = last_name + " " + first_name;

			// Load face detection and pose estimation models.
			frontal_face_detector detector = get_frontal_face_detector();
			shape_predictor pose_model;
			deserialize(landmark_model) >> pose_model;

			// Load the DNN responsible for face recognition.
			anet_type net;
			deserialize("dlib_face_recognition_resnet_model_v1.dat") >> net;

			// Detect faces 
			std::vector<rectangle> detected_faces = detector(img);

			if (detected_faces.size() != 0) { // if any face is detected we generate and save his 128D
				cout << detected_faces.size() << " FACE(s) DETECTED!" << endl;

				matrix<rgb_pixel> face_chip;
				for (auto face : detected_faces)
				{
					auto shape = pose_model(img, face);
					extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), face_chip);
				}

				matrix<float, 0, 1> face_descriptor = net(move(face_chip));

				string str = "";

				for (int i = 0; i < 128; i++) {
					str += to_string(face_descriptor(i, 0)) + ";";
				}

				stmt->executeUpdate("INSERT INTO deepvision.facedata (face_data,name) values('" + str + "', '" + full_name + "')");
				cout << "New face added to the DB!" << endl;
			}
			else {
				cout << "No face is detected in the image!" << endl;
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

