// OpenCVCam.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "PSEyeDemo.h"

using namespace std; //allows aceess to all std lib functions without using the namespace std::
using namespace cv; // allows ... without using namespace cv::


#define FRAME_RATE 130
#define RESOLUTION CLEYE_VGA
// QVGA or VGA

typedef struct{
	CLEyeCameraInstance CameraInstance;
	Mat *Frame;
	unsigned char *FramePtr;
	int Threshold;
	Mat *Temp_back;
}CAMERA_AND_FRAME;

Mat picAvg(CLEyeCameraInstance Eye, int Height, int Width);

static DWORD WINAPI CaptureThread(LPVOID ThreadPointer);

int _tmain(int argc, _TCHAR* argv[])
{

	///////MY VARS////////
	//with c++ these actually can be anywhere
	PBYTE FramePointer=NULL;
	int Width,Height,CameraCount,FramerCounter=0,Threshold=0,PicCounter=0;
	int KeyPress;
	CLEyeCameraInstance EyeCamera=NULL;
	GUID CameraID;
	Mat Frame;
	clock_t StartTime,EndTime;
	CAMERA_AND_FRAME ThreadPointer;
	HANDLE _hThread;
	string PicName;
	CLEyeCameraParameter CamCurrentParam=(CLEyeCameraParameter)0;
	bool CamParam=0;
	CamAndParam CamSet,*CamSetPtr;
	//////////////////////
	Mat Temp_back;

	//////////////////// EYE CAMERA SETUP///////////////////////////////////
	// all of this code and more is included in my header file CameraControl.h hence why its commented out
	// I left it here simply for your reference
	EyeCamera=StartCam(FRAME_RATE,RESOLUTION);//this does all the commented out code

	// Get camera frame dimensions;
	CLEyeCameraGetFrameDimensions(EyeCamera, Width, Height);
	// Create a window in which the captured images will be presented
	namedWindow( "Camera", CV_WINDOW_AUTOSIZE );
	namedWindow( "Saved Image", CV_WINDOW_AUTOSIZE );
	//Make a image to hold the frames captured from the camera
	Frame=Mat(Height,Width,CV_8UC4);//8 bit unsiged 4 channel image for Blue Green Red Alpa (8 bit elements per channel)
	 Temp_back = Mat(Height, Width, CV_8UC4);
	//Start the eye camera
	CLEyeCameraStart(EyeCamera);

	/////////////////////////////////////MAIN CODE//////////////////////////////////////



	// For high frame rate launch a seperate thread
	if(FRAME_RATE>120)
	{
		//Need to copy vars into one var to launch the second thread
		ThreadPointer.CameraInstance=EyeCamera;
		ThreadPointer.Frame = &Frame;
		ThreadPointer.Temp_back = &Temp_back;
		ThreadPointer.Threshold=0;
		//Launch thread and confirm its running
		_hThread = CreateThread(NULL, 0, &CaptureThread, &ThreadPointer, 0, 0);
		if(_hThread == NULL)
		{
			printf("failed to create thread...");
			getchar();
			return false;
		}
	}

	while( 1 ) {
		//Grab frame from camera and put image data in our frame object only if not happening in secondary thread
		/*
		if(FRAME_RATE<=60){
			CLEyeCameraGetFrame(EyeCamera,Frame.data);//capture frame and place into variable frame (frame.data() returns the memory location where the img data is stored for that variable)
			
			// Track FPS
			if(FramerCounter==0)StartTime=clock();
			FramerCounter++;
			EndTime=clock();
			if((EndTime-StartTime)/CLOCKS_PER_SEC>=1){
				cout << "FPS:" <<FramerCounter << endl;
				FramerCounter=0;
			}
			
		}
		*/

		//This will capture keypresses and do whatever you want if you assign the appropriate actions to the right key code
		KeyPress = waitKey(1);
		switch (KeyPress){
			case 27: //escape pressed
				return 0;
				break;
			case 60: // < pressed
				ThreadPointer.Threshold--;
				if(ThreadPointer.Threshold<0) ThreadPointer.Threshold=0;
				break;
			case 62: // > pressed
				ThreadPointer.Threshold++;
				if(ThreadPointer.Threshold>255) ThreadPointer.Threshold=255;
				break;
			case 115: // s save camera parameters
				SaveCameraParameters(EyeCamera);
				CLEyeCameraGetFrame(EyeCamera, Temp_back.data);
				//Temp_back = picAvg(EyeCamera, Height, Width);
				imshow( "Saved Iamge", Temp_back );
				break;
			case 108: // l load camera parameters
				LoadCameraParameters(EyeCamera);
				break;
			case 112: // p display and edit camera parameters
				CamParam=!CamParam;
				if(CamParam){
					CamSetPtr = &CamSet;
					cvSetMouseCallback("Camera",UpdateCamParam,(void*) CamSetPtr);
				}
				else cvSetMouseCallback("Camera",NullFunction);
				break;
			default: //do nothing
				break;
		}

		//This code is for real time camera parameter editing
		if(CamParam){
			//use + and - to change param to edit
			if(KeyPress=='+' || KeyPress=='=')
				CamCurrentParam=(CLEyeCameraParameter)((int)CamCurrentParam+1);
			if(KeyPress=='-')
				CamCurrentParam=(CLEyeCameraParameter)((int)CamCurrentParam-1);
			if(CamCurrentParam<0 || CamCurrentParam>19) //max param number is 19
				CamCurrentParam=(CLEyeCameraParameter)0;
			//setup struct for mouse callback function
			CamSet.Camera = EyeCamera;
			CamSet.Parameter = CamCurrentParam;
			//setup display string to be put on img
			std::stringstream ParamValue;
			ParamValue << CLEyeGetCameraParameter(EyeCamera,CamCurrentParam);
			string LeftDispString = CamParamNames[CamCurrentParam]+ ": " + ParamValue.str();
			putText(Frame,LeftDispString,Point(0,Frame.rows-5),FONT_HERSHEY_PLAIN,1,Scalar(0,255,0,0));
		}

		//Display the captured frame
		imshow( "Camera", Frame );
	}
	
	CLEyeCameraStop(EyeCamera);
	CLEyeDestroyCamera(EyeCamera);
	EyeCamera = NULL;

	return 0;
}

///////////////////////SUB THREAD///////////////////////////
//for high frame rates you will process images here the main function will allow interactions and display only
static DWORD WINAPI CaptureThread(LPVOID ThreadPointer){
	CAMERA_AND_FRAME *Instance=(CAMERA_AND_FRAME*)ThreadPointer; //type cast the void pointer back to the proper type so we can access its elements

	int FramerCounter=0;
	int itemNum = 0;
	string itemType;
	Mat Temp =Mat(Instance->Frame->rows,Instance->Frame->cols,CV_8UC4),Temp2, temp3, temp5, binImg, canImg;
	Mat temp4 = Mat(Instance->Frame->rows, Instance->Frame->cols,CV_8UC4);
	//variables used to find contour
	//CvMemStorage* storage = cvCreateMemStorage(0);

	clock_t StartTime,EndTime;
	vector< vector<Point> > contours, validContours;

	while(1){
		//Get Frame From Camera
		CLEyeCameraGetFrame(Instance->CameraInstance,Temp.data);
		// Track FPS
		if(FramerCounter==0) StartTime=clock();
		FramerCounter++;
		EndTime=clock();
		if((EndTime-StartTime)/CLOCKS_PER_SEC>=1){
			cout << "FPS:" << FramerCounter << endl;
			FramerCounter=0;
		}

		
		//Image code start
		temp3 = *(Instance->Temp_back)- Temp;
		//Convert to grayscale
		cvtColor(temp3,Temp2,CV_RGBA2GRAY);

		//Threshold image binary
		threshold(Temp2, binImg,20, 255, THRESH_BINARY);
		//threshold(Temp2,*(Instance->Frame),Instance->Threshold,0,CV_THRESH_TOZERO);
		// Canny edge detection
		Canny(binImg,canImg,50,130,3);

		//dilate(canImg, canImg,  7, Point(-1, -1), 2, 1, 1);
		//attempt 2 at contours
		 //canImg.copyTo(*(Instance->Frame) );
		findContours(canImg, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
		
		temp4 = Scalar(0, 0, 0, 0);
		//draw contours - don't need
	//	printf("vector size = %d\n", contours.size());
		validContours.clear();
		for(int i = 0; i < contours.size(); i++)
		{
			if(arcLength(contours[i], FALSE) > 270)
			{
				validContours.push_back(contours[i]);
			}
		}
		drawContours(temp4, validContours, -1, Scalar(255), CV_FILLED);
	    temp4.copyTo(*(Instance->Frame));
		//*(Instance->Frame) = temp4;
		
		

		/*
		//background subtraction
		//temp3 = Temp - *(Instance->Temp_back);
		temp3 = *(Instance->Temp_back)- Temp;

		cv::cvtColor(temp3,Temp2,CV_RGBA2GRAY);//Convert Red Green Blue Alpha image to Gray USE RGBA2BGR to color to color and drop alpha CANT CONVERT INPLACE
		//threshold(Temp2,*(Instance->Frame),Instance->Threshold,0,CV_THRESH_TOZERO);
		Canny(Temp2,Temp2,50,130,3);
		//attempt 2 at contours
		vector< vector<Point> > contours;
		findContours(Temp2, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
		//drawContours(Temp2, contours, -1, Scalar(255), CV_FILLED);
		

		threshold(Temp2, *(Instance->Frame), Instance->Threshold, 0, CV_THRESH_TOZERO);
		
		*/

		/* Attempt 1 at contours
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		RNG rng(12345);
		findContours( Temp2, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );
		// Draw contours
		for( int i = 0; i< contours.size(); i++ )
		{
			Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
			drawContours( Temp2, contours, i, color, 2, 8, hierarchy, 0, Point() );
		} 

		*/



		//Identification
/*
		switch(itemNum) {
			case 1:  itemType = "washer";
					 break;
			case 2:  itemType = "screw";
					 break;

		}
	*/
	}
	return 0;
}

Mat picAvg(CLEyeCameraInstance EyeCamera, int Height, int Width){
	Mat Temp1, Temp2, Temp3, Temp4, Avg;
	namedWindow( "Temp", CV_WINDOW_AUTOSIZE );
	Temp1 = Mat(Height, Width, CV_8UC4);
	Temp2 = Mat(Height, Width, CV_8UC4);
	Temp3 = Mat(Height, Width, CV_8UC4);
	Temp4 = Mat(Height, Width, CV_8UC4);
	Avg = Mat(Height, Width, CV_8UC4);

	//Temp1 = Temp2 = Temp3 = Temp4 = Avg =  Mat(Height, Width, CV_8UC4);
	//Temp1 = Temp2 = Temp3 = Temp4 = Avg =  Mat(Height, Width, CV_32FC3);

	CLEyeCameraGetFrame(EyeCamera, Temp1.data);
	//accumulate(Temp1, Avg);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp2.data);
	//accumulate(Temp2, Avg);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp3.data);
	//accumulate(Temp3, Avg);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp4.data);
	//accumulate(Temp4, Avg);

	Avg = Temp1+Temp2+Temp3+Temp4;

	Avg = Avg/4.0;

	imshow("Temp", Temp1);


	//Avg.convertTo(Avg,CV_8UC4);
	//Avg = mean(Avg);
	
	//normalize(Avg, Avg, 100, 255, CV_MINMAX, CV_8UC4);
	//normalize(Avg, Avg, 0, 1, 4, CV_8UC4);
	return Avg;
}
