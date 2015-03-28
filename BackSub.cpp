// OpenCVCam.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "PSEyeDemo.h"

using namespace std; //allows aceess to all std lib functions without using the namespace std::
using namespace cv; // allows ... without using namespace cv::


#define FRAME_RATE 120
#define RESOLUTION CLEYE_VGA
// QVGA or VGA

typedef struct{
	CLEyeCameraInstance CameraInstance;
	Mat *Frame;
	unsigned char *FramePtr;
	int Threshold;
	Mat *Temp_back;
}CAMERA_AND_FRAME;



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
	std::string PicName;
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
	if(FRAME_RATE>60)
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
	Mat Temp =Mat(Instance->Frame->rows,Instance->Frame->cols,CV_8UC4),Temp2, temp3;
	

	clock_t StartTime,EndTime;
	
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

		//background subtraction
		//temp3 = Temp - *(Instance->Temp_back);
		temp3 = *(Instance->Temp_back)- Temp;

		cv::cvtColor(temp3,Temp2,CV_RGBA2GRAY);//Convert Red Green Blue Alpha image to Gray USE RGBA2BGR to color to color and drop alpha CANT CONVERT INPLACE

		//threshold(Temp2,*(Instance->Frame),Instance->Threshold,0,CV_THRESH_TOZERO);
		threshold(Temp2, *(Instance->Frame), Instance->Threshold, 0, CV_THRESH_TOZERO);

		
	}
	return 0;
}
