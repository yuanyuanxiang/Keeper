
// Keeper.h : PROJECT_NAME 应用程序的主头文件
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含“stdafx.h”以生成 PCH 文件"
#endif

#include "resource.h"		// 主符号
#include <string>
using namespace std;

// 自动关闭的弹出框类型
#define MY_AfxMsgBox 6666

// CKeeperApp:
// 有关此类的实现，请参阅 Keeper.cpp
//

class CKeeperApp : public CWinApp
{
public:
	bool m_bUnique;	// 被守护程序是否单例运行

	int m_nParentId;// 守护程序的父进程ID

	bool m_bReStart; // 是否退出后重启

	bool is_debug;	 // 调试模式(允许程序降级)

	char m_strTitle[64]; // Keeper名称(小写)

	string m_strConf;	// 配置文件路径

	// 设置退出重启
	void SetReStart() { m_bReStart = true; }

	CKeeperApp();

	// 读配置、预处理
	BOOL Prepare(string &icon);

	// 找到可执行文件
	string FindExecutable(const char *sDirect);

// 重写
public:
	HANDLE m_hMutex;

	virtual BOOL InitInstance();

	virtual int ExitInstance();

	ULONG_PTR m_gdiplusToken;

	// 实现
	DECLARE_MESSAGE_MAP()
	virtual int DoMessageBox(LPCTSTR lpszPrompt, UINT nType, UINT nIDPrompt);
};

extern CKeeperApp theApp;
