﻿#include "active_control.h"

#include<iostream>
#include <process.h>

#include "Matrix.h"
#include "Log.h"
#include "data_acquisition.h"

#define FTS_TIME 0.1
using namespace Eigen;
double Force_Fc=0.3;
double Force_a=0.3;
double Force_b=1;

double anglearm=0;//手臂关节角
double angleshoul=0;//肩部关节角
double Ud_Arm=0;//力控模式算出手臂的命令速度
double Ud_Shoul=0;//力控模式算出肩部的命令速度
const char *FCH = "Dev2/ai6";//握力采集通道

activecontrol::activecontrol() {
	m_hThread = 0;
	m_stop =false;
	isMove = false;
    for(int i=0;i<2;i++)
        cmdVel[i]=0;
}
activecontrol:: ~activecontrol() { }

unsigned int __stdcall FTSThreadFun(PVOID pParam)
{
	activecontrol *FTS = (activecontrol*)pParam;
	UINT oldTickCount, newTickCount;
	oldTickCount = GetTickCount();
	while (TRUE) {
		if (FTS->m_stop) {
			break;
		}

		//延时 BOYDET_TIME s
		while (TRUE)
		{
			newTickCount = GetTickCount();
			if (newTickCount - oldTickCount >= FTS_TIME * 1000)
			{
				oldTickCount = newTickCount;
				break;
			}
			else
				SwitchToThread();
		}

		FTS->timerAcquisit();
	}
	//std::cout << "FTSThreadFun Thread ended." << std::endl;
	return 0;
}
void activecontrol::startAcquisit()
{
     //qDebug()<<"activecontrol  Start!";
 /*    mFTWrapper.LoadCalFile();
     mFTWrapper.BiasCurrentLoad(true);
     mFTWrapper.setFUnit();
     mFTWrapper.setTUnit();
	 */
	 m_stop = false;
	
	 m_hThread = (HANDLE)_beginthreadex(NULL, 0, FTSThreadFun, this, 0, NULL);
}
void activecontrol::stopAcquisit() {
	m_stop = true;

	if (m_hThread != 0) {
		::WaitForSingleObject(m_hThread, INFINITE);
		m_hThread = 0;
	}
}

void activecontrol::startMove() {
	ControlCard::GetInstance().SetMotor(MotorOn);
	ControlCard::GetInstance().SetClutch(ClutchOn);
	isMove = true;
	startAcquisit();
}

void activecontrol::stopMove() {
	ControlCard::GetInstance().SetMotor(MotorOff);
	isMove = false;
	stopAcquisit();
}

void activecontrol::timerAcquisit() {
    double readings[6] = { 0 };
    double distData[6]={0};
    double filtedData[6]={0};
	
	DataAcquisition::GetInstacne().AcquisiteSixDemensionData(readings);

	//六维力算的是白色部分受到力的方向。我们把黑色部分固定不动。当我们拉动手柄时，我们施加的力实际上是施加给了黑色部分。
	//那么白色部分就相对于黑色部分往反方向走。所以人加的力和传感器测出来的力方向是相反的。（这是由六维力安装的方式决定的）
	for (int i = 0; i < 6; ++i) {
		//if (i == 0) {
		//	readings[i] = -readings[i];
		//}
		//if (i == 3) {
		//	readings[i] = -readings[i];
		//}
		readings[i] = -readings[i];
	}
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);
	//std::cout << "f0: " << readings[0] << " f1: " << readings[1] <<
	//	" f2: " << readings[2] << " f3: " << readings[3] <<
	//	" f4: " << readings[4] << " f5: " << readings[5] << std::endl;	

    Raw2Trans(readings,distData);
    Trans2Filter(distData,filtedData);
    FiltedVolt2Vel(filtedData);
	if (isMove) {
		FTSContrl();
	}

    //qDebug()<<"readings is "<<filtedData[0]<<" "<<filtedData[1]<<" "<<filtedData[2]<<" "<<filtedData[3]<<" "<<filtedData[4]<<" "<<filtedData[5];
}
void activecontrol::Raw2Trans(double RAWData[6],double DistData[6])
{
	//这一段就是为了把力从六维力传感器上传到手柄上，这里的A就是总的一个转换矩阵。
	//具体的旋转矩阵我们要根据六维力的安装确定坐标系方向之后然后再确定。
        MatrixXd A(6,6);
		A.setZero();
        VectorXd Value_Origi(6);
        VectorXd Value_Convers(6);
		Matrix3d rotate_matrix;
		//这里的旋转矩阵要根据六维力坐标系和手柄坐标系来具体得到
		rotate_matrix <<
			cos(M_PI_4), sin(M_PI_4), 0,
			-sin(M_PI_4), cos(M_PI_4), 0,
			0, 0, 1;
		//Vector3d ForcePosition(-0.075,0.035,0);
		//手柄坐标系下手柄坐标系原点到六维力坐标系原点的向量
		Vector3d ForcePosition(0.075, -0.035, 0);
		Matrix3d ForcePositionHat;
		//这里就是这个p，我们可以想象，fx不会产生x方向的力矩，fy产生的看z坐标，fz产生的y坐标。
		//这里做的就是把力矩弄过去。这个相对坐标都是六维力坐标在手柄坐标系下的位置。
		//比如fx在y方向上有一个力臂，就会产生一个z方向上的力矩。这个力矩的方向和相对位置无关。
		//所以这个地方我们不用改这个ForcePositionHat，只用改ForcePosition这个相对位置就可以了
		ForcePositionHat <<
			0, -ForcePosition[2], ForcePosition[1],
			ForcePosition[2], 0, -ForcePosition[0],
			-ForcePosition[1], ForcePosition[0], 0;
		A.block(0, 0, 3, 3) = rotate_matrix;
		A.block(0, 3, 3, 1) = ForcePositionHat * rotate_matrix;
		A.block(3, 3, 3, 3) = rotate_matrix;


		//之前是fxfyfzMxMyMz,现在变成MxMyMzfxfyfz
        for (int i = 0; i < 6; i++) {
            if (i<3) {
                Value_Origi(i)=RAWData[i+3];
            } else {
                Value_Origi(i)=RAWData[i-3];
            }
        }
		
		//这里计算后就是
        Value_Convers = A * Value_Origi;
		//std::cout << "handle f0: " << Value_Convers(0) << " f1: " << Value_Convers(1) <<
		//	" f2: " << Value_Convers(2) << " f3: " << Value_Convers(3) <<
		//	" f4: " << Value_Convers(4) << " f5: " << Value_Convers(5) << std::endl;
        for(int m=0;m<6;m++) {
            DistData[m] = Value_Convers(m);
        }

		/*AllocConsole();
		freopen("CONOUT$", "w", stdout);*/
		//printf("fx:%lf    fy:%lf    fz:%lf \n Mx:%lf    My:%lf    Mz:%lf \n", DistData[3], DistData[4], DistData[5], DistData[0], DistData[1], DistData[2]);
}

void activecontrol::Trans2Filter(double TransData[6], double FiltedData[6]) {
    double Wc = 5;
    double Ts = 0.05;
    static int i=0;
    static double Last_Buffer[6]={0};
    static double Last2_Buffer[6]={0};
    static double Force_Buffer[6]={0};
    static double Last_FT[6]={0};
    static double Last2_FT[6]={0};
    for (int m = 0; m < 6; m++)
    {
        if (i == 0)
        {
            Last2_Buffer[m] = TransData[m];
            FiltedData[m] = 0;
            i++;
        }
        else if (i == 1)
        {
            Last_Buffer[m] = TransData[m];
            FiltedData[m] = 0;
            i++;
        }
        else
        {
            //二阶巴特沃斯低通滤波器
            Force_Buffer[m] = TransData[m];
            FiltedData[m] = (1/(Wc*Wc+2*1.414*Wc/Ts+4/(Ts*Ts)))*((Wc*Wc)*Force_Buffer[m]
            +(2*Wc*Wc)*Last_Buffer[m]
            + (Wc*Wc)*Last2_Buffer[m]
            - (2*Wc*Wc-8/(Ts*Ts))*Last_FT[m]
            -(Wc*Wc-2*1.414*Wc/Ts+4/(Ts*Ts))*Last2_FT[m]);

            Last2_FT[m] = Last_FT[m];
            Last_FT[m] = FiltedData[m];
            Last2_Buffer[m] = Last_Buffer[m];
            Last_Buffer[m] = Force_Buffer[m];
        }
    }
}

void activecontrol::FiltedVolt2Vel(double FiltedData[6]) {
    MatrixXd Vel(2,1);
    MatrixXd Pos(2,1);
    MatrixXd A(6,6);
    VectorXd Six_Sensor_Convert(6);
	double angle[2];
	ControlCard::GetInstance().GetEncoderData(angle);
    Pos(0,0)= angle[0];
    Pos(1,0)= angle[1];

	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);
	//printf("elbow angle: %lf\n", angle[1]);
	//printf("shoulder angle: %lf\n", angle[0]);
	//printf("fx:%lf    fy:%lf    fz:%lf \n Mx:%lf    My:%lf    Mz:%lf \n", FiltedData[3], FiltedData[4], FiltedData[5], FiltedData[0], FiltedData[1], FiltedData[2]);

    for (int i = 0; i < 6; i++)
    {
        Six_Sensor_Convert(i)=FiltedData[i];
    }
    damping_control(Six_Sensor_Convert,Pos,Vel,Force_Fc,Force_a,Force_b);
    Ud_Shoul = Vel(0,0);
    Ud_Arm = Vel(1,0);

	//printf("肩部速度: %lf\n", Ud_Shoul);
	//printf("肘部速度: %lf\n", Ud_Arm);

	char message_tracing[1024];
	sprintf(message_tracing, "ActiveControl, Ud_Shoul is %0.2f,Ud_Arm is %0.2f", Ud_Shoul, Ud_Arm);
	LOG1(message_tracing);
    if (Ud_Arm > 5)
    {
        Ud_Arm = 5;
    } else if (Ud_Arm < -5)
	{
		Ud_Arm = -5;
	}
    if (Ud_Shoul > 5)
    {
        Ud_Shoul = 5;
    } else if (Ud_Shoul < -5)
	{
		Ud_Shoul = -5;
	}
	

}
void activecontrol::FTSContrl() {
	ControlCard::GetInstance().VelocityMove(ShoulderAxisId, Ud_Shoul);
	ControlCard::GetInstance().VelocityMove(ElbowAxisId, Ud_Arm);
}

double activecontrol::getWirstForce()
{
	bool fireOrNot = false;
	int32       error = 0;
	TaskHandle  taskHandle = 0;
	int32       read;
	float64     data[100] = {-1};
	for (int i = 0; i < 100; i++) {
		data[i] = -1;
	}
	DAQmxCreateTask("", &taskHandle);
	DAQmxCreateAIVoltageChan(taskHandle, FCH, "",  DAQmx_Val_RSE, 0, 10.0, DAQmx_Val_Volts, NULL);
	DAQmxCfgSampClkTiming(taskHandle, "", 1000, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 100);
	DAQmxStartTask(taskHandle);
	DAQmxReadAnalogF64(taskHandle, 100, 10.0, DAQmx_Val_GroupByChannel, data, 100, &read, NULL);
	DAQmxStopTask(taskHandle);
	DAQmxClearTask(taskHandle);

	return data[50]*20;
}

bool activecontrol::isFire()
{
	bool fireOrNot = false;
	int32       error = 0;
	TaskHandle  taskHandle = 0;
	int32       read;
	float64     data[100] = {0};
	DAQmxCreateTask("", &taskHandle);
	DAQmxCreateAIVoltageChan(taskHandle, FCH, "",  DAQmx_Val_RSE, 0, 10, DAQmx_Val_Volts, NULL);
	DAQmxCfgSampClkTiming(taskHandle, "", 1000, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 100);
	DAQmxStartTask(taskHandle);
	DAQmxReadAnalogF64(taskHandle, 100, 10.0, DAQmx_Val_GroupByChannel, data, 100, &read, NULL);
	DAQmxStopTask(taskHandle);
	DAQmxClearTask(taskHandle);
	if(data[50]>0.2)
		fireOrNot = true;
	return fireOrNot;
}

void activecontrol::getEndsXY(short rangeX,short rangeY, double XY[2])
{
	MatrixXd Theta(5, 1);
	MatrixXd T0h(4, 4);
	VectorXd Pos(2);

	double angle[2] = {0};
	ControlCard::GetInstance().GetEncoderData(angle);
	Pos << -angle[0], -angle[1];
	fwd_geo_coup(Pos, Theta);
	fwd_geo_kineB(Theta, T0h);
	double x = -T0h(1, 3);
	double y = ShoulderLength + UpperArmLength + LowerArmLength - T0h(0, 3);
	
	x = std::max<double>(std::min<double>(x, 0.3), 0);
	y = std::max<double>(std::min<double>(y, 0.3), 0);
	XY[0] = (x / 0.3)*rangeX;
	XY[1] =(1 - 0.3 * y / 0.3)*rangeY;

}

void activecontrol::setDamping(float FC )
{
	Force_Fc = FC;
}