
// Keeper.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "Keeper.h"
#include "KeeperDlg.h"
#include "confReader.h"
#include "ChooseExecutable.h"
#include "Afx_MessageBox.h"

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 采用WinExec重启程序
#define WIN_EXEC 1

// CKeeperApp

BEGIN_MESSAGE_MAP(CKeeperApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CKeeperApp 构造

CKeeperApp::CKeeperApp()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
	m_bUnique = true;
	m_bReStart = false;
	m_nParentId = 0;
	m_hMutex = INVALID_HANDLE_VALUE;
}


// 唯一的一个 CKeeperApp 对象

CKeeperApp theApp;


// CKeeperApp 初始化

// 寻找可执行文件，作为被守护程序
string CKeeperApp::FindExecutable(const char *sDirect)
{
	CString filter = CString(sDirect) + _T("\\*.EXE");
	CFileFind ff;
	BOOL bFind = ff.FindFile(filter);
	vector<CString> result;
	USES_CONVERSION;
	while (bFind)
	{
		bFind = ff.FindNextFile();
		if (ff.IsDots() || ff.IsDirectory())
			continue;
		CString ret = ff.GetFileTitle();
		char *cur = W2A(ret);
		const char *lwr = _strlwr(cur);
		if (strcmp(lwr, m_strTitle))
		{
			result.push_back(ret);
		}
	}
	ff.Close();
	if (1 == result.size())
	{
		return W2A(result.at(0));
	}
	else if (result.size())
	{
		CChooseExecutable dlg(result);
		dlg.DoModal();
		return W2A(dlg.GetExecutable());
	}
	return "";
}

BOOL CKeeperApp::Prepare(string &temp)
{
	// 获取当前用户名，必须以指定身份（默认administrator）登录系统
	DWORD nLen = MAX_PATH;
	char strUser[MAX_PATH];
	if (!GetUserNameA(strUser, &nLen))
		return FALSE;

	//////////////////////////////////////////////////////////////////////////
	USES_CONVERSION;
	WCHAR wPath[MAX_PATH]; 	// 程序完整路径
	GetModuleFileName(NULL, wPath, MAX_PATH);
	CString csPath(wPath);
	int pos = csPath.ReverseFind('\\');
	CString sDir = csPath.Left(pos);
	const char * strModuleDir = W2A(sDir);
	CString sName = csPath.Right(csPath.GetLength() - pos - 1);
	pos = sName.ReverseFind('.');
	CString sTitle = sName.Left(pos);
	strcpy_s(m_strTitle, W2A(sTitle));

	//////////////////////////////////////////////////////////////////////////
	const char *pTitle = _strlwr(m_strTitle);
	m_strConf = (0 == strcmp(m_strTitle, "keeper")) ? 
		string(strModuleDir) + "\\Keep.conf" : 
		string(strModuleDir) + string("\\") + string(m_strTitle) + string(".conf");
	const string &file = m_strConf;
	// 配置文件读取
	confReader ini(file);

	// 读取模块ID
	ini.setSection("module");
	temp = ini.readStr("id", "");
	if(temp.empty())
	{
		// ID为空时读取名称
		temp = ini.readStr("name", "");
		temp = temp.empty() ? FindExecutable(strModuleDir) : temp;
		if (temp.empty())
		{
			AfxMessageBox(_T("待守护程序不存在或未被选择! 守护程序无法继续运行。"));
			return FALSE;
		}
		WritePrivateProfileStringA("module", "name", temp.c_str(), file.c_str());
		WritePrivateProfileStringA("module", "id", temp.c_str(), file.c_str());
	}

	ini.setSection("settings");
	// [14] 是否只允许唯一的实例运行（默认为1）
	m_bUnique = ini.readInt("unique", 1);

	// 用模块的唯一编码创建信号量
	m_hMutex = m_bUnique ? ::CreateMutex(NULL, TRUE, CString(temp.c_str())) : INVALID_HANDLE_VALUE;
	if ( m_bUnique && (GetLastError() == ERROR_ALREADY_EXISTS) )
	{
		if (IDCANCEL == AfxMessageBox(_T("在该设备已经运行了一个守护! ID = ") + CString(temp.c_str()) 
			+ _T(", \r\n是否继续守护? 这将重复运行被守护程序, 后果不可预料!\r\n除非你确定这2个程序不一样。"), MY_AfxMsgBox))
			return FALSE;
		int t = time(NULL) % 86400;//1天第几秒
		char t_str[64];
		sprintf_s(t_str, "_%d", t);
		temp = temp + string(t_str);
		WritePrivateProfileStringA("module", "id", temp.c_str(), file.c_str());
	}

	// [1]读取操作系统用户
	const char *dst = _strlwr(strUser);
	temp = ini.readStr("sys_user", "");
	if (temp.empty())
	{
		WritePrivateProfileStringA("settings", "sys_user", dst, file.c_str());
		temp = dst;
	}
	else if (strcmp(temp.c_str(), dst) && m_bUnique)
	{
		Afx_MessageBox box(_T("操作系统用户与配置文件不匹配! 是否重新启动程序?"));
		if (IDOK == box.DoModal())
		{
			WritePrivateProfileStringA("settings", "sys_user", dst, file.c_str());
			m_bReStart = true;
		}
		return FALSE;
	}

	// [7]读取图标文件
	Gdiplus::GdiplusStartupInput StartupInput;
	if (Gdiplus::GdiplusStartup(&m_gdiplusToken, &StartupInput, NULL))
		return FALSE;
	temp = ini.readStr("icon", "");
	temp = temp.empty() ? string(strModuleDir) + "\\Keeper.png" : temp;
	// Icon如果在当前目录
	const char *p = temp.c_str();
	if (p + 1 && ':' != p[1])
		temp = string(strModuleDir) + "\\" + temp;
	return TRUE;
}


BOOL CKeeperApp::InitInstance()
{
	float time_span = 6000.F;
	USES_CONVERSION;
	const char *id = W2A(m_lpCmdLine);
	m_nParentId = atoi(id);

	string temp;
	if (FALSE == Prepare(temp))
		return FALSE;

	// 如果一个运行在 Windows XP 上的应用程序清单指定要
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// [11]程序运行状态（程序自动生成）
	const char is_run[4] = { "1" };
	int isRun = GetPrivateProfileIntA("settings", "is_run", 0, m_strConf.c_str());
	if (1 == isRun && m_bUnique)// 上一次守护程序可能非正常关闭
	{
		Afx_MessageBox box(_T("检测到此应用程序的上一次退出是不正常的。是否检查此原因?"), time_span);
		box.DoModal();
	}else
		WritePrivateProfileStringA("settings", "is_run", is_run, m_strConf.c_str());

	AfxEnableControlContainer();

#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS
	// 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

	// 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));
#endif

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	SetRegistryKey(_T("TQEH"));

	CKeeperDlg dlg(temp);
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 在此放置处理何时用
		//  “确定”来关闭对话框的代码
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 在此放置处理何时用
		//  “取消”来关闭对话框的代码
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
		TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
	}

#ifndef _AFX_NO_MFC_CONTROLS_IN_DIALOGS
	// 删除上面创建的 shell 管理器。
	if (pShellManager != NULL)
	{
		delete pShellManager;
	}
#endif

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

int CKeeperApp::ExitInstance()
{
	// 关闭信号量句柄
	if (INVALID_HANDLE_VALUE != m_hMutex)
	{
		ReleaseMutex(m_hMutex);
		CloseHandle(m_hMutex);
		m_hMutex = INVALID_HANDLE_VALUE;
	}

	// GDI +
	Gdiplus::GdiplusShutdown(m_gdiplusToken);

	const char is_run[4] = { "0" };
	WritePrivateProfileStringA("settings", "is_run", is_run, m_strConf.c_str());

	// 重启程序
	if (m_bReStart)
	{
		char buf[_MAX_PATH];
		::GetModuleFileNameA(NULL, buf, sizeof(buf));
		CString path(buf);
		SHELLEXECUTEINFO ShExecInfo = { 0 };
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShExecInfo.lpFile = path;
		ShExecInfo.lpParameters = m_lpCmdLine;
		ShExecInfo.nShow = SW_SHOW;
#if WIN_EXEC
		if (WinExec(buf, SW_SHOW) <= 31)
#else
		if (FALSE == ShellExecuteEx(&ShExecInfo))
#endif
			OutputDebugStringA("======> 重启\"Keeper\"失败\n");
		else
			OutputDebugStringA("======> 重启\"Keeper\"成功\n");
	}
	OutputDebugStringA("======> Keeper 退出 <======\n");

	return CWinApp::ExitInstance();
}

// AfxMessageBox的重定义行为
int CKeeperApp::DoMessageBox(LPCTSTR lpszPrompt, UINT nType, UINT nIDPrompt)
{
	if (MY_AfxMsgBox == nType)
	{
		Afx_MessageBox box(lpszPrompt, 8000);

		return box.DoModal();
	}

	return CWinApp::DoMessageBox(lpszPrompt, nType, nIDPrompt);
}
