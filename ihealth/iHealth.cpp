// iHealth.cpp : �������̨Ӧ�ó������ڵ㡣
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

	//�����滹��com�����
	HRESULT Hr = ::CoInitialize(NULL);
	if (FAILED(Hr)) {
		return 0;
	}

	//��ȡ��Ļ�ֱ���
	int width = GetSystemMetrics ( SM_CXSCREEN ); 
	int height= GetSystemMetrics ( SM_CYSCREEN ); 

	char resolution[128];
	sprintf(resolution, "resolution is : width:%d, height: %d", width, height);
	SLOG1(resolution);	// DEBUGģʽ�²����ã�CSimpleLog�и���̬ʵ��

	//wke��chrome�ں˵Ĳü��棬ֻ����չʾ��JS����
	//����һ����EaWebkitҲ�����Ƶģ����µ�miniblink
	CWkeWebkitUI::WkeWebkit_Init();

	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::SetResourcePath(CPaintManagerUI::GetInstancePath() + _T("../../resource/skin"));

	RFMainWindow* pFrame = new RFMainWindow();
	if( pFrame == NULL ) return 0;
	pFrame->Create(NULL, _T("��֫���������������"), UI_WNDSTYLE_FRAME, 0L, 0, 0, RF_WINDOW_WIDTH, RF_WINDOW_HEIGHT);
	pFrame->CenterWindow();
	pFrame->SetIcon(_T("icon1.ico"));
	::ShowWindow(pFrame->GetHWND(), SW_NORMAL);
	//::SetWindowPos(pFrame->GetHWND(),HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);

	// MessageLoopӦ���ܴ�������˳���
	// �����������߳���ι�����������Ϣ��
	CPaintManagerUI::MessageLoop();

	CWkeWebkitUI::WkeWebkit_Shutdown();
	delete pFrame;
	pFrame = NULL;
	::CoUninitialize();
	return 0;
}