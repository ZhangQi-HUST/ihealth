// iHealth.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "RFMainWindow.h"
#include "WkeWebkit.h"
#include "Log.h"

long   __stdcall   ExcuteExceptionHandler(_EXCEPTION_POINTERS*   excp)   
{   
	return   EXCEPTION_EXECUTE_HANDLER;   
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int nCmdShow) {
	SetUnhandledExceptionFilter(ExcuteExceptionHandler);

	//这里面还有com组件？
	HRESULT Hr = ::CoInitialize(NULL);
	if (FAILED(Hr)) {
		return 0;
	}

	//获取屏幕分辨率
	int width = GetSystemMetrics ( SM_CXSCREEN ); 
	int height= GetSystemMetrics ( SM_CYSCREEN ); 

	char resolution[128];
	sprintf(resolution, "resolution is : width:%d, height: %d", width, height);
	SLOG1(resolution);	// DEBUG模式下不可用，CSimpleLog有个静态实例

	//wke是chrome内核的裁剪版，只用于展示和JS交互
	//还有一个叫EaWebkit也是类似的，最新的miniblink
	CWkeWebkitUI::WkeWebkit_Init();

	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::SetResourcePath(CPaintManagerUI::GetInstancePath() + _T("../../resource/skin"));

	RFMainWindow* pFrame = new RFMainWindow();
	if( pFrame == NULL ) return 0;
	pFrame->Create(NULL, _T("上肢外骨骼康复机器人"), UI_WNDSTYLE_FRAME, 0L, 0, 0, RF_WINDOW_WIDTH, RF_WINDOW_HEIGHT);
	pFrame->CenterWindow();
	pFrame->SetIcon(_T("icon1.ico"));
	::ShowWindow(pFrame->GetHWND(), SW_NORMAL);
	//::SetWindowPos(pFrame->GetHWND(),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);

	// MessageLoop应该能处理程序退出，
	// 但是其它的线程如何工作？互传消息？
	CPaintManagerUI::MessageLoop();

	CWkeWebkitUI::WkeWebkit_Shutdown();
	delete pFrame;
	pFrame = NULL;
	::CoUninitialize();
	return 0;
}