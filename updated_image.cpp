//OpenCVCam.cpp : Defines the entry point for the console application.
//

//#include <WinSock2.h>
#include "stdafx.h"
#include "PSEyeDemo.h"
#include <string.h>
//#include "xPCUDPSock.h"

using namespace std; //allows aceess to all std lib functions without using the namespace std::
using namespace cv; // allows ... without using namespace cv::


#define FRAME_RATE 130
#define RESOLUTION CLEYE_VGA
// QVGA or VGA

//#pragma pack(push,1) // Important! Tell the compiler to pack things up tightly 
//
//struct PACKOUT
//{
//	double dbl;
//	//double dbl2;
//};
//
//#pragma pack(pop) // Fall back to previous setting

typedef struct{
	CLEyeCameraInstance CameraInstance;
	Mat *Frame;
	unsigned char *FramePtr;
	int Threshold;
	Mat *Temp_back;
}CAMERA_AND_FRAME;

//functions
Mat picAvg(CLEyeCameraInstance Eye, int Height, int Width);
double fArea(double areaA[], int size);
int id(double avgA, double per, vector<double> areaArray, vector<string> strArray);
vector<string> partNames();
vector<double> partAreas(CLEyeCameraInstance EyeCamera, Mat save_img, vector<String> strArray, int rows, int cols);

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

	printf("Turn off lights and then click on the camera window and press the 's' key\n");
	printf("Click on the camera window and press the 'esc' key at any point to end\n\n"); 

	while( 1 ) {
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
				//CLEyeCameraGetFrame(EyeCamera, Temp_back.data);
				Temp_back = picAvg(EyeCamera, Height, Width);
				imshow( "Saved Image", Temp_back );
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
	clock_t StartTime,EndTime;
	vector< vector<Point> > contours, validContours;
	int FramerCounter=0;
	int itemNum = 0;
	int itemType;
	Mat Temp =Mat(Instance->Frame->rows,Instance->Frame->cols,CV_8UC4),Temp2, temp3, temp5, binImg, canImg;
	Mat Temp4 = Mat(Instance->Frame->rows, Instance->Frame->cols,CV_8UC4);
	double cArea = 0.0;
	int l = 0;
	double p1, p2, p3, p4, p5, avgA;
	double aArea[9] = {0};
	int com = 0;
	double per;  //used for average range
	Rect roi(180, 0, 300, 330);
	Mat image_roi;
	int item_seen = 0;
	int item_prev = 0;
	int item_id = 0;

	vector<string> strArray(5);  //used for part names
	vector<double> areaArray(5); //used for part areas
	int cols = Instance->Frame->cols;
	int rows = Instance->Frame->rows;

	//p1 = p2 = p3 = p4 = p5 = 0.0;
	per = 0.06;

	//need pamphlet with expected average areas
	//learning period...
	//list 1
	
	areaArray[0] = 988.00; //nut
	areaArray[1] = 1800.00; //hook
	areaArray[2] = 4400.00; //bracket
	areaArray[3] = 3500.00; // washer
	areaArray[4] = 1213.00;  // screw
	

	
	//areaArray[0] = 700.00;  
	//areaArray[1] = 850.00;
	//areaArray[2] = 3500.00;
	//areaArray[3] = 1300.00;
	//areaArray[4] = 5000.00;

	/*
	//list 2
	areaArray[0] = 600.00;  
	areaArray[1] = 770.00;
	areaArray[2] = 3400.00;
	areaArray[3] = 1500.00;
	areaArray[4] = 1300.00;
	*/
	printf("Turn on lights\n\n");
	
	strArray = partNames();
	areaArray = partAreas(Instance->CameraInstance, *(Instance->Temp_back), strArray, rows, cols);

	//set p1, p2, p3, p4, p5 = areaArray[0-4]]
	p1 = areaArray[0];
	p2 = areaArray[1];
	p3 = areaArray[2];
	p4 = areaArray[3];
	p5 = areaArray[4];

	cout<<strArray[0]<<endl;
	printf("range: %.2f -%.2f\n\n", (p1*(1-per)), (p1*(1+per)));
	cout<<strArray[1]<<endl;
	printf("range: %.2f -%.2f\n\n", (p2*(1-per)), (p2*(1+per)));
	cout<<strArray[2]<<endl;
	printf("range: %.2f -%.2f\n\n",  (p3*(1-per)), (p3*(1+per)));
	cout<<strArray[3]<<endl;
	printf("range: %.2f -%.2f\n\n", (p4*(1-per)), (p4*(1+per)));
	cout<<strArray[4]<<endl;
	printf("range: %.2f -%.2f\n\n", (p5*(1-per)), (p5*(1+per)));



	while(1){
		//Get Frame From Camera
		CLEyeCameraGetFrame(Instance->CameraInstance,Temp.data);
		// Track FPS
		if(FramerCounter==0) StartTime=clock();
		FramerCounter++;
		EndTime=clock();

		//Image code start
		temp3 = *(Instance->Temp_back)- Temp;

		//Convert to grayscale
		cvtColor(temp3,Temp2,CV_RGBA2GRAY);

		//Threshold image binary
		threshold(Temp2, binImg,20, 255, THRESH_BINARY);

		// Canny edge detection
		Canny(binImg,canImg,50,130,3);

		dilate(canImg, temp3, KERNEL_GENERAL, Point(-1, -1), 20, 1, 1);
		erode(temp3, temp5, KERNEL_GENERAL, Point(-1, -1), 20, 1, 1);

		//set region of interest
		image_roi= temp5(roi);

		//Start contours
		findContours(image_roi, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
		
		Temp4 = Scalar(0, 0, 0, 0);
		validContours.clear();
		for(int i = 0; i < contours.size(); i++)
		{
			if(arcLength(contours[i], TRUE) > 100)
			{
				validContours.push_back(contours[i]);
				cArea = contourArea(contours[i], FALSE);
				item_seen = 1;  //item seen
			}
			if(validContours.empty() && item_prev==1)
			{
				item_seen = 0;
				item_id = 0;
				l = 0;
			//	printf("\n\n");
			}			
			if((item_seen == 1) && (item_id == 0)) //seen and not id'd
			{
				if(cArea > 90 && l < 10) //collect areas
				{
		//			printf("Area[%d] = %.2f\n", l, cArea);
					aArea[l] = cArea;
					cArea = 0.0;
					l = l+1; // to limit times area is printed
				}
				if(l==10 && com == 0) 
				{
		//			printf("%d\n", l);
					avgA = fArea(aArea, l);
					printf("The average area is: %.2f\n", avgA);
					com = 1;
				}

				if(com ==1)
				{
					itemType = id(avgA, per, areaArray, strArray);
					l = 0;
					com = 0;
					item_id = 1;
				}
			}
			item_prev = item_seen;
		}

	   drawContours(Temp4, validContours, -1, Scalar(255), 5, 8, 0, 0);
       Temp4.copyTo(*(Instance->Frame));

	/*
		insert commands
	*/
	}
	return 0;
}

Mat picAvg(CLEyeCameraInstance EyeCamera, int Height, int Width){
	Mat Temp, Temp1, Temp2, Temp3, Temp4, Avg;
	namedWindow( "Temp", CV_WINDOW_AUTOSIZE );
	Temp1 = Mat(Height, Width, CV_8UC4);
	Temp1 = Mat(Height, Width, CV_8UC4);
	Temp2 = Mat(Height, Width, CV_8UC4);
	Temp3 = Mat(Height, Width, CV_8UC4);
	Temp4 = Mat(Height, Width, CV_8UC4);
	Avg = Mat(Height, Width, CV_8UC4);
	// Load image
	Mat image;

	// SetImageRoi
	// Original Width: 640 Height: 480
	//Rect roi(200, 0, Width-310, Height);
	Rect roi(180, 0, Width-340, Height-150);


	Mat image_roi;


	CLEyeCameraGetFrame(EyeCamera, Temp.data);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp1.data);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp2.data);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp3.data);
	waitKey(2);
	CLEyeCameraGetFrame(EyeCamera, Temp4.data);

	Avg = Temp1+Temp2+Temp3+Temp4;

	Avg = Avg/4.0;

	image = Avg;
	image_roi= image(roi);

	
	normalize(Avg, Avg, 100, 255, CV_MINMAX, CV_8UC4);

	imshow("Temp", image_roi);

	return Avg;
}

double fArea(double areaA[], int size)
{
	double sum, avg, percentage, dev, max, count;
	int i;
	max = areaA[0];
	sum = 0.0;
	count = 0.0;
	percentage = .12; //12%

	//find max
	for(i=0; i< size; i++)
	{
		if (areaA[i] > max)
		{
			max = areaA[i];
		}
	}
	
	printf("The max = %.2f\n", max);

	//set deviance
	dev = max - (max*percentage);
	printf("The dev = %.2f\n\n", dev);

	//compute average
	for(i=0; i<size; i++)
	{
		if (areaA[i] >= dev)
		{
			sum += areaA[i];
			count += 1.0;
		}
	}

//	printf("The sum = %.2f\n", sum);
//	printf("The count = %.2f\n", count);

	avg = sum/count;

	return avg;
}


int id(double avgA, double per, vector<double> areaArray, vector<string> strArray)
{
	//char itemType[10];
	int num;
	double p1, p2, p3, p4, p5;

	p1 = areaArray[0];
	p2 = areaArray[1];
	p3 = areaArray[2];
	p4 = areaArray[3];
	p5 = areaArray[4];

	//int nRetCode = 0;

	//Identification
	if(avgA>= (p1*(1-per)) && avgA<= (p1*(1+per)))
	{
		//strcpy(itemType, "Screw");
		//printf("Item is a %s\n\n", itemType);
		cout<<"Item is a " <<strArray[0]<<endl;
		num = 1;
		//if (!InitUDPLib())
		//	{
		//		nRetCode = 2;
		//	}else
		//	{
		//		CUDPSender sender(sizeof(PACKOUT), 12302,"127.0.0.1");
		//		PACKOUT pkout;
		//			//Sleep(1);
		//			//pkout.dbl = 0;
		//			//sender.SendData(&pkout);
		//			//Sleep(1000);
		//			Sleep(1);
		//			pkout.dbl = 1;
		//			sender.SendData(&pkout);
		//			Sleep(4000);

		//			pkout.dbl = 0;
		//			sender.SendData(&pkout);
		//			//Sleep(1000);	
		//	}
	}
	else if(avgA>= (p2*(1-per)) && avgA<=(p2*(1+per)))
	{
		//strcpy(itemType, "Nut");
		//printf("Item is a %s\n\n", itemType);
		cout<<"Item is a " <<strArray[1]<<endl;
		num = 2;
		//if (!InitUDPLib())
		//	{
		//		nRetCode = 2;
		//	}else
		//	{
		//		CUDPSender sender(sizeof(PACKOUT), 12302,"127.0.0.1");
		//		PACKOUT pkout;
		//			Sleep(1);
		//			//pkout.dbl = 0;
		//			//sender.SendData(&pkout);
		//			//Sleep(1000);

		//			pkout.dbl = 2;
		//			sender.SendData(&pkout);
		//			Sleep(4000);

		//			pkout.dbl = 0;
		//			sender.SendData(&pkout);
		//			//Sleep(1000);	
		//	}
	}
	else if(avgA>= (p3*(1-per)) && avgA<=(p3*(1+per)))
	{
		//strcpy(itemType, "Washer");
		//printf("Item is a %s\n\n", itemType);
		cout<<"Item is a " <<strArray[2]<<endl;
		num = 3;
		//if (!InitUDPLib())
		//	{
		//		nRetCode = 2;
		//	}else
		//	{
		//		CUDPSender sender(sizeof(PACKOUT), 12302,"127.0.0.1");
		//		PACKOUT pkout;
		//			Sleep(1);
		//			//pkout.dbl = 0;
		//			//sender.SendData(&pkout);
		//			//Sleep(1000);

		//			pkout.dbl = 3;
		//			sender.SendData(&pkout);
		//			Sleep(4000);

		//			pkout.dbl = 0;
		//			sender.SendData(&pkout);
		//			//Sleep(1000);	
		//	}
	}
	else if(avgA>= (p4*(1-per)) && avgA<=(p4*(1+per)))
	{
		//strcpy(itemType, "Hook");
		//printf("Item is a %s\n\n", itemType);
		cout<<"Item is a " <<strArray[3]<<endl;
		num = 4;
		//if (!InitUDPLib())
		//	{
		//		nRetCode = 2;
		//	}else
		//	{
		//		CUDPSender sender(sizeof(PACKOUT), 12302,"127.0.0.1");
		//		PACKOUT pkout;
		//			Sleep(1);
		//			//pkout.dbl = 0;
		//			//sender.SendData(&pkout);
		//			//Sleep(1000);

		//			pkout.dbl = 4;
		//			sender.SendData(&pkout);
		//			Sleep(4000);

		//			pkout.dbl = 0;
		//			sender.SendData(&pkout);
		//			//Sleep(1000);	
		//	}
	}
	else if(avgA>= (p5*(1-per)) && avgA<=(p5*(1+per)))
	{
		//strcpy(itemType, "Bracket");
		//printf("Item is a %s\n\n", itemType);
		cout<<"Item is a " <<strArray[4]<<endl;
		num = 5;
		//if (!InitUDPLib())
		//	{
		//		nRetCode = 2;
		//	}else
		//	{
		//		CUDPSender sender(sizeof(PACKOUT), 12302,"127.0.0.1");
		//		PACKOUT pkout;
		//			Sleep(1);
		//			//pkout.dbl = 0;
		//			//sender.SendData(&pkout);
		//			//Sleep(1000);

		//			pkout.dbl = 5;
		//			sender.SendData(&pkout);
		//			Sleep(4000);

		//			pkout.dbl = 0;
		//			sender.SendData(&pkout);
		//			//Sleep(1000);	
		//	}
	}
	else
	{
		//strcpy(itemType, "Junk");
		//printf("Item is %s\n\n", itemType);
		cout<<"Item is Junk" <<endl;
		num = 6;
		/*if (!InitUDPLib())
			{
				nRetCode = 2;
			}else
			{
				CUDPSender sender(sizeof(PACKOUT), 12302,"127.0.0.1");
				PACKOUT pkout;
					Sleep(1);
					pkout.dbl = 0;
					sender.SendData(&pkout);
					Sleep(1000);

					pkout.dbl = 6;
					sender.SendData(&pkout);
					Sleep(3000);

					pkout.dbl = 0;
					sender.SendData(&pkout);
					Sleep(1000);	
			}*/
	}
	printf("\n\n");
	return num;
}


vector<string> partNames()
{
	vector<string> strArray(5);
	char str[20];
	int num, ver;


	printf("\nEnter name of part 1: ");
	scanf("%s", &str);
	strArray[0] = str;
	printf("\nEnter name of part 2: ");
	scanf("%s", &str);
	strArray[1] = str;
	printf("\nEnter name of part 3: ");
	scanf("%s", &str);
	strArray[2] = str;
	printf("\nEnter name of part 4: ");
	scanf("%s", &str);
	strArray[3] = str;
	printf("\nEnter name of part 5: ");
	scanf("%s", &str);
	strArray[4] = str;

	printf("\n\nPart names are:\n");
	cout<< "1. " << strArray[0] << endl;
	cout<< "2. " <<strArray[1] << endl;
	cout<< "3. " <<strArray[2] << endl;
	cout<< "4. " <<strArray[3] << endl;
	cout<< "5. " <<strArray[4] << endl;

	printf("\nIs this correct? (1 for Yes, 0 for No) ");
	scanf("%d", &ver);

	while(ver == 0)
	{
		printf("\nPlease enter incorrect part number: ");
		scanf("%d", &num);
		switch(num)
		{	case 1: printf("\nEnter name of part 1: ");
					scanf("%s", &str);
					strArray[0] = str;
					break;
			case 2: printf("\nEnter name of part 2: ");
					scanf("%s", &str);
					strArray[1] = str;
					break;
			case 3: printf("\nEnter name of part 3: ");
					scanf("%s", &str);
					strArray[2] = str;
					break;
			case 4: printf("\nEnter name of part 4: ");
					scanf("%s", &str);
					strArray[3] = str;
					break;
			case 5: printf("\nEnter name of part 5: ");
					scanf("%s", &str);
					strArray[4] = str;
					break;
			default:  break;
		}

		printf("\n\nPart names are:\n");
		cout<< "1. " << strArray[0] << endl;
		cout<< "2. " <<strArray[1] << endl;
		cout<< "3. " <<strArray[2] << endl;
		cout<< "4. " <<strArray[3] << endl;
		cout<< "5. " <<strArray[4] << endl;
		printf("\nIs this correct? (1 for Yes, 0 for No) ");
		scanf("%d", &ver);
	}

	return strArray;
}

vector<double> partAreas(CLEyeCameraInstance EyeCamera, Mat save_img, vector<String> strArray, int rows, int cols)
{
	//Width: 640 Height: 480
	vector<double> areas(5);
	namedWindow( "Area", CV_WINDOW_AUTOSIZE );
	int ready = 0;
	Mat area;
	Rect roi(180, 0, 300, 330);
	Mat image_roi;
	Mat Temp =Mat(480, 640,CV_8UC4), temp2, temp3, temp4, temp5, temp6, temp7, temp8;
	vector< vector<Point> > contours, validContours;
	double cArea, max=0.0;
	int i, err=0, ver =0, man = 0;
	for(i = 0; i<5; i++)
	{
		while(ver==0)
		{
			cout<<"Place part "<< i+1 <<": " << strArray[i] <<" on belt" <<endl;
			while (ready == 0)
			{
				printf("Ready to find area? (1- Yes, 0- No) ");
				scanf("%d", &ready);
				printf("\n\n");
				cArea = 0.0;
				max = 0.0;
			}

			CLEyeCameraGetFrame(EyeCamera,Temp.data);  //this one is currently working
			//Temp = picAvg(EyeCamera, 480, 640);     //I think this messing up because of the normalization at the end
			//Image code start
			temp2 = save_img- Temp;

			//Convert to grayscale
			cvtColor(temp2,temp3,CV_RGBA2GRAY);

			//Threshold image binary
			threshold(temp3, temp4,20, 255, THRESH_BINARY);

			// Canny edge detection
			Canny(temp4, temp5,50,130,3);
			//
			dilate(temp5, temp6, KERNEL_GENERAL, Point(-1, -1), 20, 1, 1);
			erode(temp6, temp7, KERNEL_GENERAL, Point(-1, -1), 20, 1, 1);

			//set region of interest
			image_roi= temp7(roi);

			
			//Start contours
			findContours(image_roi, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
		
			temp8 = Scalar(0, 0, 0, 0);
			validContours.clear();
			for(int i = 0; i < contours.size(); i++)
			{
				if(arcLength(contours[i], TRUE) > 100)
				{
					validContours.push_back(contours[i]);
					cArea = contourArea(contours[i], FALSE);

					if(cArea > 90 && cArea > max) //collect areas
					{
						max = cArea;
						printf("Area = %.2f\n\n", max);
						err = 1;
					}
				}
			}

			if(err==1)
			{
				printf("Do you accept %.2f for the area of this part? (1-Yes, 0-No) ", max);
				//imshow("Area", image_roi);
				scanf("%d", &ver);
				printf("\n\n");
				ready = 0;
			}
			else
			{
				printf("Reposition part\n\n");
				//imshow("Area", image_roi);
				ready = 0;
				cArea = 0.0;
				max = 0.0;
				man += 1;

				if(man > 4)
				{
					printf("Enter manual value: ");
					scanf("%.2f", &max);
					printf("\n\n");
					ready = 0;
					ver = 1;
				}
			}
		}
		areas[i] = max;
		ver = 0;
		err = 0;
		ready = 0;
		cArea = 0.0;
		max = 0.0;
	}
	
	return areas;
}
