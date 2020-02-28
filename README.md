# Face Detection and Recognition
<p align="center">
  <img src="https://media-exp2.licdn.com/dms/image/C4D12AQG8iqV1IrnLaw/article-cover_image-shrink_720_1280/0?e=1585180800&v=beta&t=Nf_WI13HsIgDdMRi5hGEuDvN_YLezBZ2I7mOx2Ip6B4" height="350" width="500" title="hover text">
</p>

If you want learn about the theoritical part, you can read my [Article](https://www.linkedin.com/pulse/face-detection-recognition-yasser-chihab/) in order to have an idea about what's going on here !!!!

## Important !!

* The goal from this programme is to detect and recognize persons in front of a camera .

* FaceDataExtractor folder contains the source code of the program that extracts face data and store it in the database by having as an input data.txt file.

* FaceDetectionRecognition folder contains the source code of the program that detects and recognizes faces that had their data already stored.


.
## Prerequisites

In order to use this program we need a stuck of prerequisites.
* We need [Visual Studio](https://visualstudio.microsoft.com/fr/) to compile the project.

* We need to download and install [Cmake](cmake.org).

* We need to download [Dlib](http://dlib.net/) library and build using Cmake as It's explained in the official website.

* We need to download [OpenCV](https://opencv.org/) library and build using Cmake.

* We need to download [SQL connectors](https://dev.mysql.com/doc/connector-cpp/1.1/en/connector-cpp-downloading.html) in order to achieve the connection to the Database.

* We need to download and install [boost c++](https://www.boost.org/)


## Configuration

* Create a project on Visual Studio 
* After building Dlib with Cmake as I mentionned before, link it with your project ([That can helps you](http://xiaoxumeng.com/install-dlib-on-visual-studio-2015/))
* After building OpenCV with Cmake as I mentionned before, link it with your project ([That can helps you](https://www.deciphertechnic.com/install-opencv-with-visual-studio/))
* Link Sql connectors too (Figure it by your own XD)
* Link the boost c++ library too (it's easy to do it XD)

## How to use it

First of all if you want to recognize a person, you have to get his data face, that's why you need to work with the FaceDataExtractore at the first time **(I assume that you have already created the database with the schema that I provided above)**:
* Put a picture of the person in the following folder 
```
FaceDataExtractor->Release->faces

```
* Update the **data.txt** file by adding a line containing information about the person like that
```
faces/picture_name.jpg;firstName;lasttName;

```
* Update the **config.txt** file with your database server information

* Run the program with the following commande 
```
/>dataExtractor.exe  data.txt

```

After that you have stored the data of the persons that you want to recognize, you can do it now !!!

* Go the the folder that contains your program and run it like that ( don't forget to update the **config.txt** file )
```
/>faceRecognition.exe  config.txt

```

## Authors

* **Yasser Chihab**- [yasser1dev](https://github.com/yasser1dev)

## License

This project is licensed under the MIT License 

 
