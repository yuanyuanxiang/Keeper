
// KeeperDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "Keeper.h"
#include "KeeperDlg.h"
#include "afxdialogex.h"
#include <io.h>
#include "Resource.h"
#include "confReader.h"
#include "KeeperSettings.h"
#include "cmdList.h"

#if USING_GLOG
#include "log.h"
#include <direct.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#include <Tlhelp32.h>
#include <direct.h>
#include "Afx_MessageBox.h"

#include  <iostream>
#include  <fstream>
#include  <string>
#include "Downloader.h"

#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")

#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "Urlmon.lib")

#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#define __return(p) { SendMessage(WM_CLOSE); return p; }

// 全局唯一的对话框指针
CKeeperDlg *g_KeeperDlg = NULL;

// ffmpeg进程句柄
HANDLE g_ffmpeg = NULL;

// 线程计数器
NoticeNum NoticeThreaNum;

// 频繁崩溃间隔（秒）
#define TIME_SEC 60

#pragma comment(lib, "version.lib")

/************************************************************************
* @class NoticeParam
* @brief Notice结构（提示信息+声音+提示时长）
************************************************************************/
class NoticeParam 
{
private:
	const char *title;	// 窗口标题
	char *str;			// 提示信息
	char *sound;		// 声音文件
	int tm;				// 提示时长
	~NoticeParam() { delete [] str; delete [] sound; }

public:
	NoticeParam(const char *wnd, const char *src, const char *wave, int t) : 
		title(wnd),
		str(new char[strlen(src)+1]()),
		sound(new char[strlen(wave)+1]()), tm(t)
	{
		memcpy(str, src, strlen(src));
		memcpy(sound, wave, strlen(wave));
	}
	const char *window() const { return title; }	// 窗口标题
	const char *c_str() const { return str; }		// 提示信息
	const char *music() const { return sound; }		// 声音文件
	int getTime() const { return tm; }				// 提示时长
	void destroy() { delete this; }					// 调用析构函数
};

// 获取文件大小(Mb)
float GetFileSize(const char *path)
{
	CFileStatus fileStatus;
	USES_CONVERSION;
	return CFile::GetStatus(A2W(path), fileStatus) ? fileStatus.m_size / (1024.f * 1024.f) : 0;
}

// 获取exe文件的版本信息
void GetExeVersion(const char *exePath, char *version, char *strFileDescription = NULL)
{
	version[0] = 0;
	UINT sz = GetFileVersionInfoSizeA(exePath, NULL);
	if (sz)
	{
		char *pBuf = new char[sz + 1]();
		if (GetFileVersionInfoA(exePath, NULL, sz, pBuf))
		{
			VS_FIXEDFILEINFO *pVsInfo = NULL;
			if (VerQueryValueA(pBuf, "\\", (void**)&pVsInfo, &sz))
			{
				sprintf(version, "%d.%d.%d.%d", 
					HIWORD(pVsInfo->dwFileVersionMS), 
					LOWORD(pVsInfo->dwFileVersionMS), 
					HIWORD(pVsInfo->dwFileVersionLS), 
					LOWORD(pVsInfo->dwFileVersionLS));
			}
			if (strFileDescription && 
				VerQueryValueA(pBuf, "\\StringFileInfo\\080404b0\\FileDescription", (void**)&pVsInfo, &sz))
				strcpy(strFileDescription, (const char *)pVsInfo);
		}
		delete [] pBuf;
	}
}

#define POSTFIX "_update.exe" //待升级程序的后缀名称


/** 
* @brief 程序遇到未知BUG导致终止时调用此函数，不弹框
* 并且转储dump文件到当前目录.
*/
long WINAPI whenbuged(_EXCEPTION_POINTERS *excp)
{
	char path[_MAX_PATH], *p = path;
	GetModuleFileNameA(NULL, path, _MAX_PATH);
	while (*p) ++p;
	while ('\\' != *p) --p;
	time_t TIME(time(0));
	strftime(p, 64, "\\Keeper_%Y-%m-%d %H%M%S.dmp", localtime(&TIME));
	HANDLE hFile = ::CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
		FILE_ATTRIBUTE_NORMAL, NULL);
	if(INVALID_HANDLE_VALUE != hFile)
	{
		MINIDUMP_EXCEPTION_INFORMATION einfo = {::GetCurrentThreadId(), excp, FALSE};
		::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(),
			hFile, MiniDumpWithFullMemory, &einfo, NULL, NULL);
		::CloseHandle(hFile);
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

// 一种基于socket的下载器
Downloader D;

/**
* @brief 下载文件
* @param[in] name 文件名称
* @param[in] postfix 缓存文件后缀
* @param[in] isKeeper 是否守护
* @param[in] type 文件类型
*/
bool CKeeperDlg::DownloadFile(const char *name, const char *postfix, BOOL isKeeper, const char *type)
{
	const bool b64Bit = 8 == sizeof(int*);// 是否64位
	char src[_MAX_PATH];
	const char *remote = GetRemoteIp();
	if (strlen(remote) < 7)
	{
		const char *ip = ControlIp();
		D.Connect(ip, atoi(remote));
		return D.DownloadFile(name, postfix, isKeeper, type);
	}
	b64Bit ? (TRUE==isKeeper ? sprintf_s(src, "http://%s/x64/%s.%s", remote, name, type) 
		: sprintf_s(src, "http://%s/x64/%s/%s.%s", remote, m_moduleName, name, type)):
	(TRUE==isKeeper ? sprintf_s(src, "http://%s/%s.%s", remote, name, type) 
		: sprintf_s(src, "http://%s/%s/%s.%s", remote, m_moduleName, name, type));
	DeleteUrlCacheEntryA(src);
	if (isKeeper)
	{
		int nWait = rand() / (float)RAND_MAX * 2000;
		Sleep(max(nWait, 1));// 随机等待 1---2000 ms
	}
	char dst[_MAX_PATH], *p = dst;
	strcpy_s(dst, GetModulePath());
	while (*p) ++p;
	while ('\\' != *p) --p;
	sprintf(p + 1 , "%s%s", name, postfix);
	HRESULT hr = -1;
	const int times = 10; // 尝试下载的次数
	int k = times;
	do{
		hr = URLDownloadToFileA(NULL, src, dst, 0, NULL); --k;
		if (S_OK == hr || m_bExit) break;
		Sleep(20);
	}while (k);
	char szLog[300];
	sprintf_s(szLog, "======> 下载\"%s\"%s。[尝试%d次]\n", src, S_OK == hr ? "成功" : "失败", times - k);
	OutputDebugStringA(szLog);
	return (S_OK == hr && 0 == _access(dst, 0));
}

/************************************************************************
* @brief 根据"filelist.txt"下载文件
* @param[in] dst 守护程序目录
* @param[in] p 指向目录结尾的指针
* @note 文件需要以ANSI编码，否则中文名文件无法下载
************************************************************************/
void CKeeperDlg::DownloadFilelist(const char *dst, char *p)
{
	DownloadFile("filelist", ".txt", -1, "txt");// 下载filelist.txt文件
	sprintf(p + 1 , "filelist.txt");
	if (0 == _access(dst, 0)) // 根据列表下载文件
	{
		ifstream fin(dst);
		while (!fin.eof())
		{
			string str;
			getline(fin, str);
			const char *p0 = str.c_str(), *p = p0 + str.length();
			while ('.' != *p && p0 != p) --p;
			if (p0 != p)
			{
				char buf[64] = { 0 };
				memcpy(buf, p0, min(p-p0, 64));
				const char *lwr = _strlwr(buf);
				// 应用程序单独由update程序进行升级
				if (0 == strcmp(lwr, m_moduleName) || 0 == strcmp(lwr, "keeper.exe"))
					continue;
				DownloadFile(buf, p, -1, p + 1);
				Sleep(10);
			}
		}
		fin.close();
	}
}

/************************************************************************/
/* 函数说明：释放资源中某类型的文件                                     
/* 参    数：新文件名、资源ID、资源类型                                 
/* 返 回 值：成功返回TRUE，否则返回FALSE  
/* By:Koma     2009.07.24 23:30    
/* https://www.cnblogs.com/Browneyes/p/4916299.html
/************************************************************************/
BOOL CKeeperDlg::ReleaseRes(const char *strFileName, WORD wResID, const CString &strFileType)
{
	// 资源大小
	DWORD dwWrite=0;
	// 创建文件
	HANDLE hFile = CreateFileA(strFileName, GENERIC_WRITE,FILE_SHARE_WRITE,NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if ( hFile == INVALID_HANDLE_VALUE )
		return FALSE;
	// 查找资源文件中、加载资源到内存、得到资源大小
	HRSRC hrsc = FindResource(NULL, MAKEINTRESOURCE(wResID), strFileType);
	HGLOBAL hG = LoadResource(NULL, hrsc);
	DWORD dwSize = SizeofResource( NULL,  hrsc);
	// 写入文件
	WriteFile(hFile, hG, dwSize, &dwWrite, NULL);
	CloseHandle( hFile );
	return TRUE;
}

std::string GetLocalAddressBySocket(SOCKET m_socket)
{
	struct sockaddr_in m_address;
	memset(&m_address, 0, sizeof(struct sockaddr_in));
	int nAddrLen = sizeof(struct sockaddr_in);

	//根据套接字获取地址信息
	if(::getsockname(m_socket, (SOCKADDR*)&m_address, &nAddrLen) != 0)
	{
		return "";
	}

	const char* pIp = ::inet_ntoa(m_address.sin_addr);

	return pIp;
}

// Keeper升级[需要在本机开启IIS网站，且把文件复制到指定目录, 如：C:\inetpub\wwwroot]
// 1.升级程序前先尝试下载升级器"updater.exe"
// 2.升级Keeper程序时会下载"pdb"文件用于调试
// 3.升级被守护程序会下载"filelist.txt"文件，根据文件中的名称列表，逐个下载对应文件
// 注意：须在IIS配置中添加新MIME类型(如果文件类型未知)：application/octet-stream
void UpdateThread(void *param)
{
	OutputDebugStringA("======> Begin UpdateThread\n");
	const char *arg = (const char *)param;// 升级程序名
	D.SetUpdateApp(arg);
	CKeeperDlg *pThis = g_KeeperDlg;
	bool isKeeper = 0 == strcmp("Keeper", arg); // 是否升级Keeper
	
	char dst[_MAX_PATH]; // 当前目录
	strcpy_s(dst, pThis->GetModulePath());
	char *p = dst; // 指向当前目录结尾的指针
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, "updater.exe");
	if(!pThis->ReleaseRes(dst,(WORD)IDR_UPDATER, L"EXE"))
		pThis->DownloadFile("updater");// 尝试下载"updater.exe"
	do {
		Sleep(200);
		if (pThis->DownloadFile(arg, POSTFIX, isKeeper))
		{
			char up_ver[64]; // 升级程序的版本
			sprintf(p + 1 , "%s%s", arg, POSTFIX);
			GetExeVersion(dst, up_ver);
			// 版本未更新不予升级
			if ( theApp.is_debug 
				? true : strcmp(up_ver, isKeeper ? pThis->m_strKeeperVer : pThis->m_strVersion) > 0 )
			{
				theApp.is_debug = false;
				sprintf(p + 1 , "updater.exe");
				if (-1 == _access(dst, 0))
				{
					pThis->m_bUpdate = false;
					break;
				}
				CString update = CString(dst);// 执行升级程序
				CString file = CString(arg);// 升级文件
				CString params = file + _T(" ") + CString(theApp.m_lpCmdLine);// 启动参数
				SHELLEXECUTEINFO ShExecInfo = { 0 };
				ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				ShExecInfo.lpFile = update;
				ShExecInfo.nShow = SW_HIDE;
				ShExecInfo.lpParameters = isKeeper ? params : file;// 守护程序带参数
				int nWait = rand() / (float)RAND_MAX * 2000;
				Sleep(max(nWait, 200));// 随机等待 200---2000 ms
				clock_t tm = clock();
				if (ShellExecuteEx(&ShExecInfo))
				{
					if (isKeeper)
					{
						pThis->DownloadFile("Keeper", ".pdb", true, "pdb");// 下载pdb文件
						nWait = rand() / (float)RAND_MAX * 2000;
						Sleep(max(nWait, 200));// 随机等待 200---2000 ms
						pThis->DownloadFile("ffmpeg", ".exe", true, "exe");// 下载ffmpeg文件
						nWait = rand() / (float)RAND_MAX * 2000;
						Sleep(max(nWait, 200));// 随机等待 200---2000 ms
						pThis->m_bUpdate = false;
						pThis->ExitKeeper(false);// 退出守护
						tm = clock() - tm;
						TRACE("======> Exit Keeper Using: %d ms.\n", tm);
					}else
					{
						char notice[_MAX_PATH];
						sprintf_s(notice, "正在升级应用程序\"%s\"，请勿进行任何操作，否则可能导致升级失败。"
							"升级完成后将自动启动应用程序。", pThis->GetAppName());
						pThis->Notice(notice, 8000);
						pThis->Stop(true);
						WaitForSingleObject(ShExecInfo.hProcess, 30000);
						pThis->DownloadFilelist(dst, p);
						pThis->Start();
						pThis->m_bUpdate = false;
					}
				}else 
				{
					pThis->m_bUpdate = false;
					OutputDebugStringA("======> ShellExecuteEx updater失败.\n");
				}
			}else 
			{
				pThis->DownloadFilelist(dst, p);
				pThis->m_bUpdate = false;
				OutputDebugStringA("======> 升级文件的版本不高于目标程序, 不予升级.\n");
			}
		}else{
			pThis->DownloadFilelist(dst, p);
			pThis->m_bUpdate = false;
		}
	}while(false);
	D.Disconnect();
	OutputDebugStringA("======> End UpdateThread\n");
}

/************************************************************************
* @brief 还原目标目录的文件
* @param[in] backup 备份文件目录("\\"结尾)
* @param[in] dstDir 被还原的目标文件目录("\\"结尾)
************************************************************************/
void recovery(const std::string &backup, const std::string &dstDir)
{
	//文件句柄
	intptr_t hFile = 0;
	//文件信息  
	struct _finddata_t fileinfo;
	std::string s, dst = dstDir;
	BOOL bSuccess = TRUE;
	try
	{
		if ((hFile = _findfirst(s.assign(backup).append("*.*").c_str(), &fileinfo)) != -1)
		{
			do{
				_strlwr(fileinfo.name);
				if (IS_DIR == fileinfo.attrib)
				{
					// 子目录
					if(strcmp(fileinfo.name, ".") && strcmp(fileinfo.name, "..")){
						recovery(backup + fileinfo.name + "\\", dstDir + fileinfo.name + "\\");
					}
				}
				else if (strcmp(fileinfo.name, ".") && strcmp(fileinfo.name, "..")
					&& strcmp(fileinfo.name, "keeper.exe"))
				{
					std::string cur = s.assign(backup).append(fileinfo.name);
					std::string d = dst.append(fileinfo.name);
					for(int k = 100; !DeleteFileA(d.c_str()) && --k; ) Sleep(200);
					if(FALSE == MoveFileA(cur.c_str(), d.c_str())) // 开始还原，移动文件
					{
						TRACE("======> 还原文件失败: %s\n", d.c_str());
						bSuccess = FALSE;
					}
					DeleteFileA(cur.c_str());
				}
			} while (_findnext(hFile, &fileinfo) == 0);
			_findclose(hFile);
		}
		RemoveDirectoryA(backup.c_str());
	}catch (std::exception e){ if(hFile) _findclose(hFile); }
}

// 程序还原
void RecoverThread(void *param)
{
	OutputDebugStringA("======> Begin RecoverThread\n");

	const char *arg = (const char *)param;// 降级程序名
	CKeeperDlg *pThis = g_KeeperDlg;
	char dst[_MAX_PATH]; // 当前目录
	strcpy_s(dst, pThis->GetModulePath());
	char *p = dst; // 指向当前目录结尾的指针
	while (*p) ++p;
	while ('\\' != *p) --p;
	sprintf(p+1, ".old\\%s.exe", arg);
	if (0 == _access(dst, 0))
	{
		char notice[_MAX_PATH];
		sprintf_s(notice, "正在对应用程序\"%s\"降级，请勿进行任何操作，否则可能导致还原失败。"
			"降级完成后将自动启动应用程序。", arg);
		pThis->Notice(notice, 8000);
		pThis->Stop(true);
		*(p+1) = 0;
		std::string root(dst); // 还原文件所在目录
		strcpy(p+1, ".old\\");
		std::string logDir(dst);// 备份文件所在目录
		recovery(logDir, root);
		pThis->Start();
	}else
	{
		OutputDebugStringA("======> 不需要还原被守护程序.\n");
		pThis->SendInfo("提示", "没有备份文件，不需要还原被守护程序。");
	}

	pThis->m_bUpdate = false;
	OutputDebugStringA("======> End RecoverThread\n");
}

// FILETIME转time_t
time_t FileTime2TimeT(FILETIME ft)
{
	ULARGE_INTEGER ui;
	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;
	return (ui.QuadPart - 116444736000000000) / 10000000;
}

// 获取进程启动时间
int GetStartTime(HANDLE _hProcess)
{
	FILETIME creation_time, exit_time, kernel_time, user_time;
	if (GetProcessTimes(_hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
	{
		time_t TM = FileTime2TimeT(creation_time);
		tm *date = localtime(&TM);
		char szLog[64];
		strftime(szLog, 64, "%Y-%m-%d %H:%M:%S", date);
		TRACE("======> 被守护程序已经启动: %s\n", szLog);
		return TM;
	}
	return time(NULL);
}

// 获取父进程ID：https://blog.csdn.net/shaochat/article/details/38731365
ULONG_PTR GetParentProcessId(int pid) 
{
	ULONG_PTR pbi[6], id = (ULONG_PTR)-1;
	ULONG ulSize = 0;
	LONG (WINAPI *NtQueryInformationProcess)(HANDLE ProcessHandle, ULONG ProcessInformationClass,
		PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);
	*(FARPROC *)&NtQueryInformationProcess = GetProcAddress(LoadLibraryA("NTDLL.DLL"), "NtQueryInformationProcess");

	if(NtQueryInformationProcess){
		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
		if(NtQueryInformationProcess(hProcess, 0, &pbi, sizeof(pbi), &ulSize) >= 0 && ulSize == sizeof(pbi))
			id = pbi[5];
		CloseHandle(hProcess);
	}
	return id;
}


// 安全的取得真实系统信息
void SafeGetNativeSystemInfo(__out LPSYSTEM_INFO lpSystemInfo)
{
	typedef void (WINAPI *LPFN_GetNativeSystemInfo)(LPSYSTEM_INFO lpSystemInfo);
	LPFN_GetNativeSystemInfo fnGetNativeSystemInfo = (LPFN_GetNativeSystemInfo)
		GetProcAddress(GetModuleHandle(_T("kernel32")), "GetNativeSystemInfo");
	if (NULL != fnGetNativeSystemInfo)
	{
		fnGetNativeSystemInfo(lpSystemInfo);
	}
	else
	{
		GetSystemInfo(lpSystemInfo);
	}
}

// 获取操作系统位数
int GetSystemBits()
{
	SYSTEM_INFO si;
	SafeGetNativeSystemInfo(&si);
	return (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||  
		si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ) ? 64 : 32;
}

// 仅op为真时才提示64位
DWORD GetProcessId(const CString &processName, const CString &strFullPath, BOOL &op)
{
	DWORD id = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe = { 0 };
	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnapshot, &pe))
		return FALSE;
	while (TRUE == Process32Next(hSnapshot, &pe))
	{
		id = 0;
		_wcslwr_s(pe.szExeFile);
		if (pe.szExeFile == processName)
		{
			id = pe.th32ProcessID;
			MODULEENTRY32 me = { 0 };
			me.dwSize = sizeof(MODULEENTRY32);
			HANDLE hModule = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, id);
			if (op && ERROR_PARTIAL_COPY == GetLastError())
			{
				if(CheckWowProcess(hModule) && 64==GetSystemBits())
				{
					AfxMessageBox(L"请使用64位守护程序, 否则可能导致意想不到的结果!"
						, MB_ICONINFORMATION | MB_OK);
				}
				CloseHandle(hModule);
				op = FALSE;
				break;
			}
			Module32First(hModule, &me);
			CloseHandle(hModule);
			_wcslwr_s(me.szExePath);
			if (me.szExePath == strFullPath)
				break;
		}
	}
	CloseHandle(hSnapshot);
	return id;
}

// 返回值op：0表示失败、程序需退出，非0表示成功、程序继续运行
HANDLE CKeeperDlg::GetProcessHandle(const CString &processName, const CString &strFullPath, BOOL &op)
{
	DWORD id = GetProcessId(processName, strFullPath, op);
	return id ? OpenProcess(PROCESS_ALL_ACCESS, FALSE, id) : NULL;
}

// 获取进程的线程数量
int GetThreadCount(DWORD th32ProcessID)
{
	int count = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	PROCESSENTRY32 pe = { 0 };
	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnapshot, &pe))
		return 0;
	while (TRUE == Process32Next(hSnapshot, &pe))
	{
		if (pe.th32ProcessID == th32ProcessID)
		{
			count = pe.cntThreads;
			break;
		}
	}
	CloseHandle(hSnapshot);

	return count;
}


const char* GetLocalHost()
{
	static char localhost[128] = { "127.0.0.1" };
	char hostname[128] = { 0 };
	if (0 == gethostname(hostname, 128))
	{
		hostent *host = gethostbyname(hostname);
		// 将ip转换为字符串
		char *hostip = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
		memcpy(localhost, hostip, strlen(hostip));
	}
	return localhost;
}


// 去掉窗口的关闭按钮
HWND DeleteCloseMenu(HWND hWnd)
{
	if (hWnd)
	{
		HMENU pMenu = GetSystemMenu(hWnd, FALSE);
		if (pMenu)
		{
			EnableMenuItem(pMenu, SC_CLOSE, MF_GRAYED | MF_BYCOMMAND);
			DrawMenuBar(hWnd);
		}
	}

	return hWnd;
}

// 添加窗口的关闭按钮
HWND AddCloseMunu(HWND hWnd)
{
	if (hWnd)
	{
		::ShowWindow(hWnd, SW_SHOW);
		HMENU pMenu = GetSystemMenu(hWnd, FALSE);
		if (pMenu)
		{
			EnableMenuItem(pMenu, SC_CLOSE, MF_ENABLED | MF_BYCOMMAND);
			DrawMenuBar(hWnd);
		}
	}

	return hWnd;
}

void 
	GetCurrentPath(char* path)
{
	char cCurrentDir[_MAX_PATH]={0};
	DWORD dword=::GetModuleFileNameA(NULL, cCurrentDir, _MAX_PATH);

	DWORD dwCount=dword;
	while(dwCount>0)
	{
		dwCount--;
		if(cCurrentDir[dwCount]==0x5c)
			break; 
		else
		{
			cCurrentDir[dwCount]=0;
		}
	}

	strcpy(path, cCurrentDir);
}

void InitLog()
{
	// 如果日志目录不存在，则创建
	char m_sLogPath[_MAX_PATH];
	GetCurrentPath(m_sLogPath);
	strcat(m_sLogPath, "log");
	if (_access(m_sLogPath, 0) == -1)
		_mkdir(m_sLogPath);

#if USING_GLOG
	
	// init log lib
	const char *app = theApp.m_strTitle;

	InitGoogleLogging(app);

	char cInfoPath[_MAX_PATH] = { 0 };
	sprintf(cInfoPath, "%s\\%sLog_", m_sLogPath, app);
	//日志实时输出
	FLAGS_logbufsecs = 0;          
	// 日志大于此值时，创建新的日志
	FLAGS_max_log_size = 2;       
	//当磁盘被写满时，停止日志输出
	FLAGS_stop_logging_if_full_disk = true;  
	// 关闭写日志到err
	FLAGS_alsologtostderr = false;

	google::SetLogDestination(google::GLOG_INFO, cInfoPath);
	google::SetLogDestination(google::GLOG_WARNING, cInfoPath);
	google::SetLogDestination(google::GLOG_ERROR, cInfoPath);

	logInfo << "<<< Keeper Start. >>>";

#endif
}


void unInitLog()
{
#if USING_GLOG

	logInfo << "<<< Keeper Stop. >>>";

	google::ShutdownGoogleLogging();

#endif
}


// 获取操作系统版本[Windows 10版本为100及以上]
int getOsVersion()
{
	OSVERSIONINFO info = {};
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&info);
	DWORD dwMajor = info.dwMajorVersion;
	DWORD dwMinor = info.dwMinorVersion;
	return 10 * dwMajor + dwMinor;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CKeeperDlg 对话框



CKeeperDlg::CKeeperDlg(const string& Icon, CWnd* pParent)
	: CDialog(CKeeperDlg::IDD, pParent)
{
	m_bExit = TRUE;
#undef new
	m_strIcon = CString(Icon.c_str());
	pBmp = NULL;
	m_hIcon = NULL;

	InitLog();
	InitializeCriticalSection(&m_cs);
	m_finder = NULL;
	m_strCreateTime[0] = 0;
	m_strModeTime[0] = 0;
	m_fFileSize = 0;
	strcpy_s(m_strVersion, "Unknown");
	m_bIsStoped = S_RUN;
	m_bUpdate = false;
	m_nAliveTime = ALIVE_TIME;
	memset(m_strUpServer, 0, sizeof(m_strUpServer));
}


CKeeperDlg::~CKeeperDlg()
{
	unInitLog();
	DeleteCriticalSection(&m_cs);
}


void CKeeperDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CKeeperDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_BN_CLICKED(IDC_BUTTON_EXIT, &CKeeperDlg::OnOK)
	ON_BN_CLICKED(IDC_BUTTON_SHOWCONSOLE, &CKeeperDlg::OnBnClickedButtonShowconsole)
	ON_COMMAND(ID_APP_ABOUT, &CKeeperDlg::OnAppAbout)
	ON_MESSAGE(WM_TRAY_MSG, &CKeeperDlg::OnTrayCallbackMsg)
	ON_COMMAND(ID_EXIT_MENU, &CKeeperDlg::OnExitMenu)
	ON_COMMAND(ID_SELF_START, &CKeeperDlg::OnSelfStart)
	ON_UPDATE_COMMAND_UI(ID_SELF_START, &CKeeperDlg::OnUpdateSelfStart)
	ON_WM_INITMENUPOPUP()
	ON_WM_WINDOWPOSCHANGING()
	ON_COMMAND(ID_SETTINGS, &CKeeperDlg::OnSettings)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CKeeperDlg 消息处理程序


BOOL CKeeperDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
		DeleteMenu(pSysMenu->GetSafeHmenu(), SC_CLOSE, MF_BYCOMMAND);// 删除关闭按钮
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作

	m_bTray = FALSE;
	m_trayPopupMenu.LoadMenu(IDR_POP_MENU);

	// TODO: 在此添加额外的初始化代码
	// 获取当前程序路径
	USES_CONVERSION;
	GetModuleFileNameA(NULL, m_pKeeperPath, MAX_PATH);
	CString csPath(m_pKeeperPath);
	int pos = csPath.ReverseFind('\\');
	CString sDir = csPath.Left(pos);
	const char * strModuleDir = W2A(sDir);
	sprintf_s(m_sLogDir, "%s\\log", strModuleDir);
	CWnd *pWnd = GetDlgItem(IDC_EDIT_CUREXE);
	pWnd->SetWindowText(csPath);
	WSADATA wsaData; // Socket
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	//////////////////////////////////////////////////////////////////////////
	// [必须]读取模块信息
	m_strConf = theApp.m_strConf;
	SetFileAttributesA(m_strConf.c_str(), FILE_ATTRIBUTE_HIDDEN);
	confReader ini(m_strConf.c_str());
	ini.setSection("module");
	string id = ini.readStr("id", "");
	string name = ini.readStr("name", "UnKnown");
	string pwd = ini.readStr("password", "admin");
	strcpy_s(m_password, pwd.c_str());
	// [1]参看Keeper.cpp
	// [2]观测时间
	ini.setSection("settings");
	m_nWatchTime = max(ini.readInt("watch_time", 50), 1);
	// [3]是否开机启动
	m_bAutoRun = ini.readInt("auto_run", 1);
	int run = timeGetTime()/1000; // 开机时间（秒）
	const int delay = 100; // 延时（秒）
	if(0 == m_bAutoRun && run < delay) // 非开机自启，开机一段时间内不准启动程序
	{
		CString tips;
		tips.Format(_T("开机已运行%ds, 请于%.ds后重试!"), run, delay - run);
		Afx_MessageBox box(tips, 1000*(delay - run));
		box.DoModal();
		__return(FALSE);
	}
	// [4]启动时守护程序是否可见
	m_nVisible = ini.readInt("visible", 1);
	// [5]绑定CPU
	m_nAffinityCpu = max(ini.readInt("cpu", 0), 0);
	// [6]读取模块标题
	string title = ini.readStr("title", "");
	// [7]参看Keeper.cpp
	// [8]远端地址
	string remote = ini.readStr("remote", "");
	if (remote.empty()) 
	{
		remote = GetLocalHost();
		strcpy(m_Ip, remote.c_str());
		if (!remote.empty())
		{
			char *p = m_Ip;
			while(*p) ++p;
			while('.' != *p && p != m_Ip) --p;
			*(p+1) = 'X'; *(p+2) = 0;
			CServerFinder::SetWaitTime(60);
		}
	}else 
		strcpy(m_Ip, remote.c_str());
	// [9]远端端口
	m_nPort = ini.readInt("port", 9999);
	m_pSocket = 0==strcmp(m_Ip, "0") ? NULL : new CBcecrSocket();
	// [10]退出代码
	m_nExitCode = ini.readInt("exit_code", 0);

	//////////////////////////////////////////////////////////////////////////
	InitRemoteIp(m_Ip, m_nPort);
	//////////////////////////////////////////////////////////////////////////
	if(id.length() > 32 || name.length() > 32)
	{
		MessageBox(_T("配置文件模块信息的字段名称超长!"), _T("错误"), MB_ICONERROR);
		__return(FALSE);
	}
	strcpy(m_moduleId, id.c_str());
	strcpy(m_moduleName, name.c_str());
	m_bKeeeperExit = TRUE;
	m_bCheckExit = TRUE;
	m_bSocketExit = TRUE;
	m_bExit = FALSE;
	CMenu *pMenu = GetSystemMenu(FALSE);
	pMenu->ModifyMenu(SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
	pMenu->EnableMenuItem(SC_CLOSE, MF_DISABLED);
	// 设置程序开机自启动
	char pRegName[64] = { 0 };// 注册表项目名称
	sprintf(pRegName, "Keep_%s", m_moduleId);
	if(FALSE == SetSelfStart(m_pKeeperPath, pRegName, m_bAutoRun) && m_bAutoRun)
	{
		MessageBox(csPath + _T("\r\n设置开机自启动失败!"), _T("错误"), MB_ICONERROR);
	}

	SetWindowText(_T("Keep - ") + CString(m_moduleName));
	sprintf(m_modulePath, "%s\\%s.exe", strModuleDir, m_moduleName);
	strcpy(m_sTitle, title.empty() ? m_modulePath : title.c_str());
	_strlwr_s(m_moduleName);
	SetDlgIcon(m_strIcon); // 设置图标
	sprintf_s(m_FileDescription, "%s.exe", m_moduleName);
	_strlwr_s(m_modulePath);
	GetFileInfo();
	GetExeVersion(m_pKeeperPath, m_strKeeperVer);
	// 初始化名称数组
	InitTitles(m_sTitle);
	// 进行守护
	if (INVALID_HANDLE_VALUE == CreateThread(0, 0, keepProc, this, 0, NULL))
		__return(FALSE);
	if (INVALID_HANDLE_VALUE == CreateThread(0, 0, checkProc, this, 0, NULL))
		__return(FALSE);
	if (m_pSocket && INVALID_HANDLE_VALUE == CreateThread(0, 0, socketProc, this, 0, NULL))
		__return(FALSE);

	m_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	pWnd = GetDlgItem(IDC_EDIT_KEEPEXE);
	pWnd->SetWindowText(A2W(m_modulePath));
	m_ThreadId = 0;
	m_nRunTimes = 0;
	m_sRunLog[0] = '\0';
	// [13] 是否显示守护程序图标（默认为1）
	int show_icon = ini.readInt("show_icon", 1);
	if (show_icon)
		HideToTray();
	g_KeeperDlg = this;
	srand(time(NULL));

	// 崩溃时写dump文件
	SetUnhandledExceptionFilter(&whenbuged);

	// [12]定时记录程序信息（分钟）
	int bLog = ini.readInt("log", 10);
	if(bLog > 0) SetTimer(1, bLog*60*1000, NULL);

	return TRUE;
}


void CKeeperDlg::InitTitles(const char *t)
{
	CString wd = CString(t);
	int nOsVersion = getOsVersion();
	m_sWindowNames[0] = wd;
	if (nOsVersion > WINDOWS_7_VER)
	{
		m_sWindowNames[1] = L"选择" + wd;
		m_sWindowNames[2] = L"标记" + wd;
		m_sWindowNames[3] = L"选定 " + wd;
		m_sWindowNames[4] = L"标记 " + wd;
	}
	else
	{
		m_sWindowNames[1] = L"选定 " + wd;
		m_sWindowNames[2] = L"标记 " + wd;
		m_sWindowNames[3] = L"选择" + wd;
		m_sWindowNames[4] = L"标记" + wd;
	}
}

/************************************************************************
* @brief 在指定目录找寻一个图片，返回其路径
* 优先查找顺序：png, jpg, ico, bmp
************************************************************************/
std::string findIconFile(const char *dir)
{
	//文件句柄
	intptr_t hFile = 0;
	//文件信息  
	struct _finddata_t fileinfo;
	std::string s, ret;
	try
	{
		if ((hFile = _findfirst(s.assign(dir).append("*.*").c_str(), &fileinfo)) != -1)
		{
			do{
				_strlwr(fileinfo.name);
				if (IS_DIR == fileinfo.attrib)
				{
					continue;
				}
				else if (strcmp(fileinfo.name, ".") && strcmp(fileinfo.name, ".."))
				{
					if(strstr(fileinfo.name, ".png") || strstr(fileinfo.name, ".jpg")
						|| strstr(fileinfo.name, ".ico") || strstr(fileinfo.name, ".bmp")){
						ret = s.assign(dir).append(fileinfo.name);
						break;
					}
				}
			} while (_findnext(hFile, &fileinfo) == 0);
			_findclose(hFile);
		}
	}catch (std::exception e){ if(hFile) _findclose(hFile); }

	return ret;
}

/************************************************************************
* @brief 为被守护程序设置图标
* 第一顺序：读取配置文件种的图标路径
* 第二顺序：获取当前目录下的一张图片
* 第三顺序：以程序名字生成图片
************************************************************************/
void CKeeperDlg::SetDlgIcon(const CString &icon)
{
	if (pBmp) return;
	if (FALSE == PathFileExists(icon)){
		char buf[_MAX_PATH], *p = buf;
		strcpy_s(buf, m_pKeeperPath);
		while (*p) ++p;
		while ('\\' != *p) --p;
		*(p + 1) = 0;
		std::string file = findIconFile(buf);
		if (file.empty())
		{
			// 图标文字
			CString text(m_moduleName);
			text.MakeUpper();
			int n = strlen(m_moduleName);
			if (n<=4)
			{
				pBmp = new Gdiplus::Bitmap(64, 64, PixelFormat32bppARGB);
				Gdiplus::Graphics graphics(pBmp);
				graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
				graphics.FillRectangle(&Gdiplus::SolidBrush(Gdiplus::Color(0, 0, 0, 0)), Gdiplus::Rect(0, 0, 64, 64));
				Gdiplus::FontFamily fontFamily(L"黑体");
				Gdiplus::Font font(&fontFamily, Gdiplus::REAL(n >= 3 ? 24 : 32), Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
				Gdiplus::StringFormat strformat;
				strformat.SetAlignment(Gdiplus::StringAlignmentCenter);
				strformat.SetLineAlignment(Gdiplus::StringAlignmentCenter);
				graphics.DrawString(text, -1, &font, Gdiplus::RectF(0, 16, 64, 32), &strformat, 
					&Gdiplus::SolidBrush(Gdiplus::Color(255, 0, 0, 255)));
			}else 
				pBmp = NULL;
		}else{
			pBmp = new Gdiplus::Bitmap(CString(file.c_str()));
		}
	}else{
		pBmp = new Gdiplus::Bitmap(m_strIcon);
	}
	Gdiplus::Status STATUS = pBmp ? pBmp->GetHICON(&m_hIcon) : Gdiplus::Status::FileNotFound;
	m_hIcon = STATUS == Gdiplus::Status::Ok ? m_hIcon 
		: AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标
}

// 播放声音
#include <mmsystem.h>
#pragma comment( lib, "Winmm.lib" )

// 通过线程发布公告，因为消息无响应
void NoticeThread(void *param)
{
	OutputDebugStringA("======> BEGIN NoticeThread\n");

	NoticeParam *np = (NoticeParam*)param;
	CString notice(np->c_str());
	CString sound(np->music());
	const char *title = np->window();
	int tm = np->getTime();
	np->destroy();
	if(!PlaySound(sound, NULL, SND_FILENAME | SND_ASYNC))
		DeleteFile(sound);
	else Sleep(500);
	Afx_MessageBox box(CString(notice), tm);
	box.SetTitle(title);
	box.SetNotice();
	box.DoModal();
	PlaySound(NULL, NULL, NULL);
	NoticeThreaNum.AddNoticeNum(-1);

	OutputDebugStringA("======> END NoticeThread\n");
}

void CKeeperDlg::Notice(const char *notice, int tm)
{
	char sound[_MAX_PATH], *p = sound;
	GetModuleFileNameA(NULL, sound, _MAX_PATH);
	while(*p) ++p;
	while ('\\' != *p) --p; ++p;
	strcpy(p, "Notice.wav");
	if (-1 == _access(sound, 0))
	{
		ReleaseRes(sound, (WORD)IDR_NOTICE, L"WAVE");
	}
	NoticeThreaNum.AddNoticeNum(1);
	_beginthread(&NoticeThread, 0, new NoticeParam(m_moduleName, notice, sound, tm));
}

// ffmpegThread线程参数
struct ffmpegThreadParam
{
	int m_bThreadState;		// 线程状态（TRUE:运行中 FALSE:已停止）
	int m_nPort;			// UDP端口（奇数表示传送被守护程序界面）
	ffmpegThreadParam() : m_bThreadState(FALSE), m_nPort(0) { }
};

// 传屏线程
void ffmpegThread(void *param)
{
	OutputDebugStringA("======> BEGIN ffmpegThread\n");

	ffmpegThreadParam *Para = (ffmpegThreadParam*)param;
	if (g_KeeperDlg)
		g_KeeperDlg->ffmepgStart(Para->m_nPort);
	Para->m_bThreadState = FALSE;

	OutputDebugStringA("======> END ffmpegThread\n");
}

// 停止控制台程序（首先尝试寻找句柄，发送关闭消息）
bool StopConsoleApp(const char *app, HANDLE hProcess)
{
	char ffmpeg[_MAX_PATH], *p = ffmpeg;
	GetModuleFileNameA(NULL, ffmpeg, _MAX_PATH);
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, app);
	HWND h = ::FindWindowA("ConsoleWindowClass", ffmpeg);
	bool bFind = false;
	if (h) // 尝试正常关闭控制台程序
	{
		bFind = true;
		::SendMessage(h, WM_CLOSE, 0, 0);
	}else if (hProcess)
		TerminateProcess(hProcess, -1);
	if (hProcess)
		CloseHandle(hProcess);
	return bFind;
}

/************************************************************************
// 将被守护程序运行界面传送到远程控制器
// nPort: UDP端口，取值如下：
// 0- 停止传送
// 奇数- 传送被守护程序界面
// 偶数- 传送桌面
// < _BASE_PORT - 启动ghost
************************************************************************/
void CKeeperDlg::Watch(int nPort)
{
	static ffmpegThreadParam param; // 线程参数
	HWND hWnd = FindKeepWindow();// 被守护程序的句柄

	if (FALSE == param.m_bThreadState && nPort > 0)
	{
		if (hWnd && nPort >= _BASE_PORT)
		{
			::ShowWindow(hWnd, SW_SHOWNORMAL);
			::SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
		}
		param.m_nPort = nPort;
		param.m_bThreadState = TRUE;
		_beginthread(&ffmpegThread, 0, &param);
	}else if (nPort == 0)
	{
		// 监控端主动关闭
		ffmpegStop("ffmpeg.exe");
		if (hWnd)
		{
			::SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
		}
	}
	if (nPort < 0)
	{
		if(!StopConsoleApp("ghost.exe", NULL))
		{
			// ghost有可能提权
			char ffmpeg[_MAX_PATH] = {"管理员: "}, *p = ffmpeg + strlen(ffmpeg);
			GetModuleFileNameA(NULL, p, _MAX_PATH - strlen(ffmpeg));
			while (*p) ++p;
			while ('\\' != *p) --p;
			strcpy(p+1, "ghost.exe");
			HWND h = ::FindWindowA("ConsoleWindowClass", ffmpeg);
			if (h) // 尝试正常关闭控制台程序
			{
				::SendMessage(h, WM_CLOSE, 0, 0);
			}
		}
	}
}

// 如果当前目录下面存在"ffmpeg.exe"，则将被守护程序运行界面传送到远程控制器
// nPort 奇数: 传送被守护程序界面
// nPort 偶数: 传送整个桌面
// < _BASE_PORT: 启动ghost
void CKeeperDlg::ffmepgStart(int nPort)
{
	char ffmpeg[_MAX_PATH], *p = ffmpeg;
	GetModuleFileNameA(NULL, ffmpeg, _MAX_PATH);
	while (*p) ++p;
	while ('\\' != *p) --p;
	const char *server = nPort >= _BASE_PORT ? "ffmpeg" : "ghost";
	sprintf(p+1, "%s.exe", server);
	if (nPort < _BASE_PORT)
		ReleaseRes(ffmpeg, (WORD)IDR_GHOST, L"EXE");
	if (0 == _access(ffmpeg, 0))
	{
		int x = 0, y = 0;// 传屏起始位置
		int w = ::GetSystemMetrics(SM_CXSCREEN);  //屏幕宽度
		int h = ::GetSystemMetrics(SM_CYSCREEN);  //屏幕高度
		HWND hWnd = NULL;
		if ( (nPort >= _BASE_PORT) && (nPort&1) && (hWnd = FindKeepWindow()) )
		{// 将被守护程序移动到合适位置，如果没有找到句柄，则传送整个桌面
			CRect rect;
			if(::GetWindowRect(hWnd, &rect))
			{
				x = rect.left, y = rect.top;
				w = min(rect.Width(), w);
				h = min(rect.Height(), h);
				if (x < 0 || y < 0 || rect.right > w || rect.bottom > h)
				{
					x = y = 0;
					::MoveWindow(hWnd, x, y, w, h, TRUE);
				}
			}
		}
		// 拼接命令（25fps，H264格式）
		char param[512];
		(nPort >= _BASE_PORT) ? 
			sprintf_s(param, "-f gdigrab -framerate 25 -offset_x %d -offset_y %d -video_size %dx%d"\
			" -i desktop -vcodec libx264 -preset:v ultrafast -tune:v zerolatency -f h264 udp://%s:%d",
			x, y, w, h, ControlIp(), nPort) : sprintf_s(param, "%s %d", ControlIp(), nPort);
		CString lpFile = CString(ffmpeg);
		CString lpParameters = CString(param);
		SHELLEXECUTEINFO ShExecInfo = { 0 };
		ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShExecInfo.hwnd = NULL;
		ShExecInfo.lpVerb = NULL;
		ShExecInfo.lpFile = lpFile;
		ShExecInfo.lpParameters = lpParameters;
		ShExecInfo.lpDirectory = NULL;
#ifdef _DEBUG
		ShExecInfo.nShow = SW_SHOW;
#endif
		ShExecInfo.hInstApp = NULL;
		if (FALSE == ShellExecuteEx(&ShExecInfo)){
			TRACE("===> [ERROR] 启动 \"%s\" 失败。\n", server);
			if (nPort >= _BASE_PORT)
			{
				SendInfo("ffmpeg", "启动\"ffmpeg\"失败，无法传送屏幕!");
			}
		}else {
			TRACE("===> [SUCCESS] 启动 \"%s\" 成功。\n", server);
			clock_t run = clock();
			if (nPort >= _BASE_PORT)
			{
				g_ffmpeg = ShExecInfo.hProcess;
				if(WAIT_OBJECT_0 == WaitForSingleObject(g_ffmpeg, 60 * 60 * 1000))
					OutputDebugStringA("===> [INFO] 退出 \"ffmpeg\" 成功。\n");
				int n = clock() - run;
				if(n < 3000)
				{
					DWORD code = 0;// 退出代码
					GetExitCodeProcess(g_ffmpeg, &code);
					SendInfo("ffmpeg", "\"ffmpeg\"启动成功，但无法捕获屏幕!", code);
				}
			}else
			{
				WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
				CloseHandle(ShExecInfo.hProcess);
				DeleteFileA(ffmpeg);
			}
		}
	}else{
		TRACE("===> [INFO] 程序 \"%s\" 不存在。\n", server);
		if (nPort >= _BASE_PORT)
		{
			SendInfo("ffmpeg", "程序\"ffmpeg\"不存在，无法传送屏幕!");
		}
	}
}

// 停止ffmpeg传送屏幕
void CKeeperDlg::ffmpegStop(const char *app)
{
	Lock();
	HANDLE hProcess = g_ffmpeg;
	g_ffmpeg = NULL;
	Unlock();

	if(hProcess)
	{
		StopConsoleApp(app, hProcess);
	}
}

void CKeeperDlg::InitRemoteIp(const char *ip, int port)
{
	if (NULL == m_pSocket)
		return;

	char buf[64];
	memcpy(buf, ip, 64);
	char *x = buf;
	while (*x) ++x; --x;
	// 如果IP形式为：192.168.12.X，则在整个网段搜索设备
	if ('x' == *x || 'X' == *x)
	{
		while (buf != x && ('x' == *x || 'X' == *x))
			--x;
		*(x+1) = 0;
		if (NULL == m_finder)
			m_finder = new CServerFinder(buf, port, 4);
		else
			m_finder->SetIpPort(buf, port);
	}else
	{
		if (m_finder)
			m_finder->SetThreadWait();
	}
}


BOOL CKeeperDlg::SetTray(BOOL bTray)
{
	BOOL bRet = FALSE;

	NOTIFYICONDATA tnd;
	tnd.cbSize = sizeof(NOTIFYICONDATA);
	tnd.hWnd = m_hWnd;
	tnd.uID = IDR_MAINFRAME;

	if (bTray)
	{
		tnd.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
		tnd.uCallbackMessage = WM_TRAY_MSG;
		tnd.hIcon = m_hIcon;
		// 最小化托盘时鼠标移到图标时的提示语
		CString tip = _T("正在守护程序:\r\n") + CString(m_modulePath);
		_tcscpy_s(tnd.szTip, sizeof(tnd.szTip), tip);
		bRet = Shell_NotifyIcon(NIM_ADD, &tnd);
	}
	else
	{
		bRet = Shell_NotifyIcon(NIM_DELETE, &tnd);
	}
	return bRet;
}


// 获取文件的创建日期、修改日期、版本号
void CKeeperDlg::GetFileTimeInfo(const char *file)
{
	if (_access(file, 0) == -1)
		return;

	HANDLE h = CreateFileA(file, NULL, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (INVALID_HANDLE_VALUE != h)
	{
		FILETIME fCreate, fWrite;
		if(GetFileTime(h, &fCreate, NULL, &fWrite))
		{
			SYSTEMTIME sysTime;
			FileTimeToSystemTime(&fCreate, &sysTime);
			sysTime.wHour += 8;
			sprintf_s(m_strCreateTime, "%04d/%02d/%02d %02d:%02d:%02d", sysTime.wYear, 
				sysTime.wMonth, sysTime.wHour >= 24 ? sysTime.wDay + 1 : sysTime.wDay, 
				sysTime.wHour >= 24 ? sysTime.wHour - 24 : sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
			FileTimeToSystemTime(&fWrite, &sysTime);
			sysTime.wHour += 8;
			sprintf_s(m_strModeTime, "%04d/%02d/%02d %02d:%02d:%02d", sysTime.wYear, 
				sysTime.wMonth, sysTime.wHour >= 24 ? sysTime.wDay + 1 : sysTime.wDay, 
				sysTime.wHour >= 24 ? sysTime.wHour - 24 : sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
		}
		CloseHandle(h);
	}
}


void CKeeperDlg::GetFileInfo() // 获取被守护程序文件信息
{
	GetFileTimeInfo(m_modulePath);
	GetExeVersion(m_modulePath, m_strVersion, m_FileDescription);
	m_fFileSize = GetFileSize(m_modulePath);
}


void CKeeperDlg::GetDiskInfo(char *info)
{
	info[0] = 0;
	char disk[4] = { m_modulePath[0], m_modulePath[1], 0, 0};
	unsigned __int64 freeBytesToCaller, totalBytes, freeBytes;
	if (TRUE == GetDiskFreeSpaceExA(disk, (PULARGE_INTEGER)&freeBytesToCaller, 
		(PULARGE_INTEGER)&totalBytes, (PULARGE_INTEGER)&freeBytes))
	{
		sprintf(info, "%d/%d G", int(freeBytes>>30), int(totalBytes>>30));
	}
}


BOOL CKeeperDlg::HideToTray()
{
	BOOL ret = FALSE;
	ShowWindow(SW_HIDE);
	if (!m_bTray)
	{
		ret = SetTray(TRUE);
		m_bTray = FALSE;
	}
	return ret;
}


// 停止程序并关机
void CKeeperDlg::Shutdown()
{
	OutputDebugStringA("======> Shutdown\n");

	// 退出被守护程序
	ExitKeeper(true);

	// 5s后关机
	system("shutdown -s -t 10");
	// 等keepProc退出
	for (int K = 200; !m_bKeeeperExit && --K; )
		Sleep(10);
	m_bSocketExit = TRUE;
	// 关闭本程序
	SendMessage(WM_CLOSE, 0, 0);
}


// 重启计算机
void CKeeperDlg::ReBoot()
{
	OutputDebugStringA("======> ReBoot\n");

	// 退出被守护程序
	ExitKeeper(true);

	// 5s后重启
	system("shutdown -r -t 10");
	// 等keepProc退出
	for (int K = 200; !m_bKeeeperExit && --K; )
		Sleep(10);
	m_bSocketExit = TRUE;
	// 关闭本程序
	SendMessage(WM_CLOSE, 0, 0);
}

// 重启被守护的程序(bForce - 是否强制重启)
bool CKeeperDlg::ReStart(bool bForce)
{
	OutputDebugStringA("======> ReStart\n");

	HWND hWnd = FindKeepWindow();
	if (NULL == hWnd || false == theApp.m_bUnique)
		bForce = true;

	if (bForce)
	{
		if (m_ThreadId)
		{
			char cmd[128];
			sprintf(cmd, "taskkill /f /pid %d", m_ThreadId);
			system(cmd);
			return true;
		}
	}
	else
	{
		::SendMessage(hWnd, WM_CLOSE, 0, 0);
		return true;
	}
	return false;
}


/** 
Keeper向管理终端上报状态
<?xml version="1.0" encoding="GB2312" standalone="yes"?>
<request command="keepAlive">
<parameters>
	<nAliveTime>%d</nAliveTime>
	<szName>%s</szName>
	<szCpu>%s</szCpu>
	<szMem>%s</szMem>
	<szThreads>%s</szThreads>
	<szHandles>%s</szHandles>
	<szRunLog>%s</szRunLog>
	<szRunTimes>%s</szRunTimes>
	<szCreateTime>%s</szCreateTime>
	<szModTime>%s</szModTime>
	<szFileSize>%.2fM</szFileSize>
	<szVersion>%s</szVersion>
	<szKeeperVer>%s</szKeeperVer>
	<szCmdLine>%s</szCmdLine>
	<szDiskFreeSpace>%s</szDiskFreeSpace>
	<szStatus>%s</szStatus>
	<szCurrentTime>%s</szCurrentTime>
</parameters>
</request>
*/
void CKeeperDlg::Refresh()
{
	OutputDebugStringA("======> Refresh\n");

	if (NULL == m_pSocket || !m_pSocket->IsConnected())
		return;

	static std::string IP = GetLocalAddressBySocket(m_pSocket->getSocket());
	static const char *ip = IP.c_str();

	if (m_pSocket->IsRegistered())
	{
		AppInfo info;
		strcpy_s(info.ip, ip);
		strcpy_s(info.name, m_moduleName);
		strcpy_s(info.run_log, log_runtime());
		sprintf_s(info.run_times, "%d", m_nRunTimes);
		if(QueryAppInfo(info))
		{
			const int PACKAGE_LENGTH = 2048;
			char buffer[PACKAGE_LENGTH + 256] = { 0 }, xml[PACKAGE_LENGTH] = { 0 };
			SYSTEMTIME st;
			GetLocalTime(&st);
			char ct[64];// current time
			sprintf_s(ct, "%d,%d,%d,%d,%d,%d,%d,%d", st.wYear, st.wMonth, st.wDayOfWeek, 
				st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

			sprintf_s(xml, 
				"<?xml version=\"1.0\" encoding=\"GB2312\" standalone=\"yes\"?>\r\n"
				"<request command=\"%s\">\r\n"
				"  <parameters>\r\n"
				"    <nAliveTime>%d</nAliveTime>\r\n"
				"    <szName>%s</szName>\r\n"
				"    <szCpu>%s</szCpu>\r\n"
				"    <szMem>%s</szMem>\r\n"
				"    <szThreads>%s</szThreads>\r\n"
				"    <szHandles>%s</szHandles>\r\n"
				"    <szRunLog>%s</szRunLog>\r\n"
				"    <szRunTimes>%s</szRunTimes>\r\n"
				"    <szCreateTime>%s</szCreateTime>\r\n"
				"    <szModTime>%s</szModTime>\r\n"
				"    <szFileSize>%.2fM</szFileSize>\r\n"
				"    <szVersion>%s</szVersion>\r\n"
				"    <szKeeperVer>%s</szKeeperVer>\r\n"
				"    <szCmdLine>%s</szCmdLine>\r\n"
				"    <bits>%s</bits>\r\n"
				"    <szDiskFreeSpace>%s</szDiskFreeSpace>\r\n"
				"    <szStatus>%s</szStatus>\r\n"
				"    <szCurrentTime>%s</szCurrentTime>\r\n"
				"  </parameters>\r\n"
				"</request>\r\n\r\n", KEEPALIVE, m_nAliveTime, info.name, info.cpu, info.mem, 
				info.threads, info.handles, info.run_log, info.run_times, m_strCreateTime, 
				m_strModeTime, m_fFileSize, m_strVersion, m_strKeeperVer, m_modulePath, info.bits, 
				info.disk_info, info.status, ct);
			sprintf_s(buffer, SIP_RequestHeader_i(ip, m_Ip, KEEPER_VERSION, KEEPER_DATETIME, (int)strlen(xml)));
			strcat_s(buffer, xml);
			m_pSocket->sendData(buffer, strlen(buffer));
		}
	}else
	{
		char buffer[512] = { 0 };
		GetRegisterPkg(buffer, ip, m_Ip);
		m_pSocket->sendData(buffer, strlen(buffer));
	}
}

/** 
Keeper向管理终端发送消息
<?xml version="1.0" encoding="GB2312" standalone="yes"?>
<request command="infomation">
<parameters>
	<code>%d</code>
	<info>%s</info>
	<details>%s</details>
</parameters>
</request>
*/
void CKeeperDlg::SendInfo(const char *info, const char *details, int code)
{
	char buffer[2048], xml[2048];
	sprintf_s(xml, 
		"<?xml version=\"1.0\" encoding=\"GB2312\" standalone=\"yes\"?>\r\n"
		"<request command=infomation>\r\n"
		"  <parameters>\r\n"
		"    <code>%d</code>\r\n"
		"    <info>%s</info>\r\n"
		"    <details>%s</details>\r\n"
		"  </parameters>\r\n"
		"</request>\r\n\r\n", code, info, details);
	static std::string IP = GetLocalAddressBySocket(m_pSocket->getSocket());
	static const char *ip = IP.c_str();
	sprintf_s(buffer, SIP_RequestHeader_i(ip, m_Ip, KEEPER_VERSION, KEEPER_DATETIME, (int)strlen(xml)));
	strcat_s(buffer, xml);
	m_pSocket->sendData(buffer, strlen(buffer));
}

void CKeeperDlg::Stop(bool bAll)
{
	if (S_STOP != m_bIsStoped)
	{
		OutputDebugStringA("======> Stop\n");
		m_bIsStoped = S_STOP;
		m_bIsStoped = StopApp(false);
		if (bAll) // 将另外的副本程序强行停止
		{
			BOOL op = FALSE;
			DWORD id = NULL;
			const CString processName = CString(m_moduleName) + L".exe";
			while (id = GetProcessId(processName, CString(m_modulePath), op))
			{
				char cmd[128];
				sprintf(cmd, "taskkill /f /pid %d", id);
				system(cmd);
			}
		}
	}
}


void CKeeperDlg::Start()
{
	if (S_RUN != m_bIsStoped)
	{
		OutputDebugStringA("======> Start\n");
		GetFileInfo();
		m_bIsStoped = S_RUN;
	}
}


/************************************************************************
* @brief 接收远程升级指令，有如下3种形式
* update:pos 从指定位置升级守护程序
* update:a/+,pos 从指定位置升级被守护程序
* update:- 还原被守护程序
************************************************************************/
void CKeeperDlg::Update(const std::string &arg)
{
	if (0 == strcmp("keeper", theApp.m_strTitle) &&
		false == m_bUpdate)
	{
		OutputDebugStringA("======> Update\n");
		bool isKeeper = true; // 是否升级守护
		bool isUp = true; // 升级或降级
		if (arg.length() > 1)
		{
			const char *p = arg.c_str();
			if ('a' == p[0] || '+' == p[0])
			{
				isKeeper = false;
				p += 2;
			}
			strcpy_s(m_strUpServer, p);
		}else if(0 == strcmp(arg.c_str(), "-"))
		{
			isKeeper = false;
			isUp = false;
		}else return; // 参数不足
		const char *app = isKeeper ? "Keeper" : m_moduleName;
		m_bUpdate = true;
		_beginthread(isUp ? &UpdateThread : &RecoverThread, 0, (void *)app);
	}
}


void CKeeperDlg::SetTime(const std::string &arg)
{
	OutputDebugStringA("======> SetTime\n");
	char buf[64];
	strcpy_s(buf, arg.c_str());
	int n[8] = {0}, i = 0;
	for(char *p = strtok(buf, ","); NULL != p && i < 8; p = strtok(NULL, ","))
		n[i++] = atoi(p);
	SYSTEMTIME st = { n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7] };
	BOOL result = (8 == i ? SetLocalTime(&st) : FALSE);
	TRACE("======> 设置系统时间%s.\n", result ? "成功" : "失败");
}


bool CKeeperDlg::QueryAppInfo(AppInfo &info)
{
	BOOL b32 = FALSE;
	Lock();
	float use = m_cpu.get_cpu_usage();
	sprintf_s(info.cpu, "%.2f%%", min(use, 1.f) * 100.f);
	sprintf_s(info.mem, "%.2fM", m_cpu.get_mem_usage());
	sprintf_s(info.handles, "%d", m_cpu.get_handles_count());
	sprintf_s(info.bits, m_handle && IsWow64Process(m_handle, &b32) ? (b32 ? "32位" : "64位") : "Unknown");
	GetDiskInfo(info.disk_info);
	Unlock();
	sprintf_s(info.threads, "%d", GetThreadCount(GetProcessId(m_handle)));
	DWORD_PTR dwResult = 0;
	HWND hWnd = FindKeepWindow();
	if (hWnd)
	{
		// 在此添加检测窗口是否正常的代码
		BOOL isHung = IsHungAppWindow(hWnd);
		if (isHung) sprintf_s(info.status, "异常");
	}else sprintf_s(info.status, "未检测");

	return true;
}


const char* CKeeperDlg::log_runtime(bool bLog2File)
{
	Lock();
	if (S_RUN == m_bIsStoped || bLog2File)
	{
		time_t TIME = mytick.time();
		double temp = TIME > 86400 ? TIME/86400.0 : (TIME > 3600 ? TIME/3600.0 : TIME);
		sprintf_s(m_sRunLog, "%.2f %s", temp, TIME > 86400 ? "天" : (TIME > 3600 ? "时" : "秒"));
	}else
	{
		strcpy_s(m_sRunLog, S_STOP == m_bIsStoped ? "已停止" : "已暂停");
	}
	Unlock();
	return m_sRunLog;
}


void CKeeperDlg::log_command(const time_t *TM, const char *ip, const char *cmd, const char *arg) const
{
	const tm *date = localtime(TM);
	char szLog[512];
	strftime(szLog, 512, "%Y-%m-%d %H:%M:%S", date);
	char filename[_MAX_PATH];
	sprintf_s(filename, "%s\\%d年%d月监控日志.txt", m_sLogDir, 
		1900 + date->tm_year, 1 + date->tm_mon);
	FILE *m_fp = fopen(filename, "a+");
	if (m_fp)
	{
		sprintf_s(szLog, "%s %s: %s %s\n", szLog, ip, cmd, arg);
		fwrite(szLog, strlen(szLog), 1, m_fp);
		fclose(m_fp);
	}
}


void CKeeperDlg::log_command(const time_t *TM, const char *content) const
{
	const tm *date = localtime(TM);
	char szLog[512];
	strftime(szLog, 512, "%Y-%m-%d %H:%M:%S", date);
	char filename[_MAX_PATH];
	sprintf_s(filename, "%s\\%d年%d月运行日志.txt", m_sLogDir, 
		1900 + date->tm_year, 1 + date->tm_mon);
	FILE *m_fp = fopen(filename, "a+");
	if (m_fp)
	{
		sprintf_s(szLog, "%s: %s\n", szLog, content);
		fwrite(szLog, strlen(szLog), 1, m_fp);
		fclose(m_fp);
	}
}

// 响应函数处理
LRESULT CKeeperDlg::OnTrayCallbackMsg(WPARAM wparam, LPARAM lparam)
{
	switch(lparam)
	{
	case WM_LBUTTONDOWN: // 左键单击
		{
			OnBnClickedButtonShowconsole();
		}
		break;
	case WM_RBUTTONDOWN: // 右键弹出菜单
		{
			CMenu *pMenu = NULL;
			CPoint pt;
			pMenu = m_trayPopupMenu.GetSubMenu(0);
			GetCursorPos(&pt);
			SetForegroundWindow();// 菜单在失去焦点时关闭
			pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, this);
			break;
		}
	default:
		break;
	}
	return 0;
}


void CKeeperDlg::OnExitMenu()
{
	SendMessage(WM_CLOSE, 0, 0);
}


void CKeeperDlg::OnDestroy()
{
	CDialog::OnDestroy();

	// m_bExit确保子线程退出
	m_bExit = TRUE;

	// 停止搜索
	if (m_finder)
	{
		m_finder->Exit();
		delete m_finder;
	}

	// 等待守护线程
	while (!m_bKeeeperExit || !m_bCheckExit || !m_bSocketExit || m_bUpdate)
		Sleep(10);
	SetTray(FALSE);
	if (pBmp)
	{
		delete pBmp;
		pBmp = NULL;
	}
	if (g_ffmpeg)
	{
		TerminateProcess(g_ffmpeg, -2);
		SendInfo("ffmpeg", "因守护程序退出，停止传送屏幕!");
		Sleep(20);
	}
	if (m_pSocket)
	{
		m_pSocket->unInit();
		delete m_pSocket;
	}
	WSACleanup();
	OutputDebugStringA("======> Keeper退出成功。\n");
}

/// 获取本机IP
std::string CKeeperDlg::GetLocalHost() const
{
	char hostname[_MAX_PATH] = { 0 };
	if (0 == gethostname(hostname, sizeof(hostname)))
	{
		hostent *host = gethostbyname(hostname);
		// 将ip转换为字符串
		char *hostip = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
		return hostip;
	}
	return "";
}

void CKeeperDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else if (nID == SC_MINIMIZE)
	{
		HideToTray();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CKeeperDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CKeeperDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// 删除注册表值为"strVal"的开机自启动项
LSTATUS RegDeleteValueX(HKEY hKey, const char *strVal)
{
	for (int i = 0; ; ++i)
	{
		char strName[_MAX_PATH], pValue[_MAX_PATH];
		DWORD nLen = _MAX_PATH, vLen = _MAX_PATH, kType = 0;
		if (RegEnumValueA(hKey, i, strName, &nLen, 0, &kType, (LPBYTE)pValue, &vLen))
			break;
		TRACE("[%d]: Name = %s, Val = %s\n", i, strName, pValue);
		if (REG_SZ == kType)
		{
			if (0 == strcmp(pValue, strVal))
				return RegDeleteValueA(hKey, strName);
		}
	}
	return -1;
}

/** 
* @brief 设置本身开机自启动
* @param[in] *sPath 注册表的路径
* @param[in] *sNmae 注册表项名称
* @param[in] bEnable 是否开机自启
* @return 返回注册结果
* @details Win7 64位机器上测试结果表明，注册项在：\n
* HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
*/
BOOL CKeeperDlg::SetSelfStart(const char *sPath, const char *sNmae, bool bEnable) const
{
	// 写入的注册表路径
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// 在注册表中写入启动信息
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// 判断是否成功
	if(lRet != ERROR_SUCCESS)
		return FALSE;
	
	lRet = bEnable ?
		RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1)
		: ( ERROR_SUCCESS == RegDeleteValueA(hKey, sNmae) ? ERROR_SUCCESS : RegDeleteValueX(hKey, sPath) );

	// 关闭注册表
	RegCloseKey(hKey);

	// 判断是否成功
	return lRet == ERROR_SUCCESS;
}

DWORD WINAPI CKeeperDlg::keepProc(LPVOID pParam)
{
	CKeeperDlg *pThis = (CKeeperDlg*)pParam;
	pThis->m_bKeeeperExit = FALSE;
	OutputDebugStringA("keepProc 线程已启动。\n");
	// 守护的程序完整路径
	USES_CONVERSION;
	CString Module = A2W(pThis->m_modulePath);
	// 检查守护程序是否存在
	if (_access(pThis->m_modulePath, 0) == -1)
	{
		pThis->EnableWindow(FALSE);
		pThis->ShowWindow(SW_HIDE);
		if (IDYES == AfxMessageBox(Module + _T("\r\n不存在! 是否重置?"), MB_ICONQUESTION | MB_YESNO))
		{
			// 清空ID和NAME
			WritePrivateProfileStringA("module", "id", "", theApp.m_strConf.c_str());
			WritePrivateProfileStringA("module", "name", "", theApp.m_strConf.c_str());
			theApp.SetReStart();
		}
		pThis->m_bExit = TRUE;
		pThis->m_bKeeeperExit = TRUE;
		OutputDebugStringA("keepProc 线程已退出。\n");
		pThis->SendMessage(WM_CLOSE, 0, 0);
		return 0xdead001;
	}

	// 如果程序频繁崩溃，超过一定次数将不进行守护
	int count = 0;
	const int K_max = 10;
	const CString processName = CString(pThis->m_moduleName) + L".exe";
	BOOL op = TRUE;
	pThis->m_handle = theApp.m_nParentId 
		? OpenProcess(PROCESS_ALL_ACCESS, FALSE, theApp.m_nParentId) 
		: pThis->GetProcessHandle(processName, CString(pThis->m_modulePath), op);
	bool bFind = pThis->m_handle ? true : false;
	if (op)
	{
		// 无限循环，监视守护进程
		do{
			if (pThis->m_bIsStoped)
			{
				Sleep(20);
				continue;
			}

			// 守护程序如果是被动启动，则不需要再次启动被守护程序
			if ((0 == pThis->m_handle || false==theApp.m_bUnique) && 0==theApp.m_nParentId )
			{
				CString lpParameters;
				lpParameters.Format(_T("-k %d"), int(GetCurrentProcessId()));
				SHELLEXECUTEINFO ShExecInfo = { 0 };
				ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				ShExecInfo.hwnd = NULL;
				ShExecInfo.lpVerb = NULL;
				ShExecInfo.lpFile = Module;
				ShExecInfo.lpParameters = lpParameters;// 该参数具体指明被守护程序被谁守护
				ShExecInfo.lpDirectory = NULL;
				ShExecInfo.nShow = pThis->IsVisible() ? SW_SHOW : SW_HIDE;
				ShExecInfo.hInstApp = NULL;
				if (TRUE == ShellExecuteEx(&ShExecInfo))
				{
					if( pThis->m_nAffinityCpu && // 若该值设置为0，将不绑定某个CPU内核
						!SetProcessAffinityMask(ShExecInfo.hProcess, 1<<(pThis->m_nAffinityCpu - 1)) )
						OutputDebugString(Module + _T("\r\n绑定CPU失败!"));
					pThis->m_handle = ShExecInfo.hProcess;
					if(CheckWowProcess(ShExecInfo.hProcess) && 64==GetSystemBits())
					{
						pThis->Notice("请使用64位守护程序, 否则可能导致意想不到的结果!");
					}
				}
			}
			
			// 若允许运行多个被守护程序，则在第2,3,...被守护程序启动完成时，守护程序即退出
			if( (bFind && false == theApp.m_bUnique) && 0 == theApp.m_nParentId )
				break;

			pThis->Lock();
			pThis->mytick.set_beginTime(GetStartTime(pThis->m_handle));
			pThis->m_ThreadId = ::GetProcessId(pThis->m_handle);
			pThis->m_cpu.setpid(pThis->m_ThreadId);
			pThis->m_nRunTimes ++;
			pThis->Refresh();
			pThis->Unlock();

			// 控制台程序特殊处理
			HWND hWnd = NULL;
			for(int n = 200; --n && !pThis->m_bExit; )
			{
				if (hWnd = pThis->FindKeepWindow())
					break;
				Sleep(30);
			}
			if (hWnd)
			{
				DeleteCloseMenu(hWnd);
				::ShowWindow(hWnd, pThis->IsVisible() ? SW_SHOW : SW_HIDE);
				::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)pThis->GetIcon());
				::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)pThis->GetIcon());
			}

			// 等待进程退出
			const HANDLE handles[] = {pThis->m_handle, pThis->m_event};
			WaitForMultipleObjects(2, handles, FALSE, INFINITE);
			bFind = FALSE;
			theApp.m_nParentId = 0;// 被守护进程退出，守护程序主动启动它

			// 如果2次启动之间小于TIME_SEC，且频繁重启，超过一定次数关闭守护
			time_t TIME = pThis->mytick.time();
			count = (TIME < TIME_SEC) ? count + 1 : 0;
			if (count == K_max)
			{
				pThis->EnableWindow(FALSE);
				pThis->ShowWindow(SW_HIDE);
				CString strPrompt = Module + _T("\r\n频繁崩溃或重启! 是否退出守护?");
				Afx_MessageBox box(strPrompt, 1000 * TIME_SEC);
				if (IDCANCEL == box.DoModal())
				{
					count = K_max - 2;
					pThis->m_handle = 0;
					continue;
				}
				break;
			}
			if (0 == pThis->m_handle)
				continue;

			time_t TM = time(NULL);
			struct tm *date = localtime(&TM);
			char szLog[512];
			strftime(szLog, 512, "%Y-%m-%d %H:%M:%S", date);
			sprintf_s(szLog, "%s [%s:%u]持续运行时间: %s.%s%s\n", szLog, pThis->m_moduleName, 
				pThis->m_ThreadId, pThis->log_runtime(true), pThis->m_bExit ? "[守护退出]" : "", 
				pThis->m_bIsStoped ? "[远程停止]" : "");
#if USING_GLOG
			logInfo << szLog;
#else 
			OutputDebugStringA(szLog);
			char filename[_MAX_PATH];
			sprintf_s(filename, "%s\\%d年%d月守护日志.txt", pThis->m_sLogDir, 
				1900 + date->tm_year, 1 + date->tm_mon);
			FILE *m_fp = fopen(filename, "a+");
			if (m_fp)
			{
				fwrite(szLog, strlen(szLog), 1, m_fp);
				fclose(m_fp);
			}
#endif

			DWORD code = 0;// 退出代码
			GetExitCodeProcess(pThis->m_handle, &code);
			CloseHandle(pThis->m_handle);
			// 如果处于人为停止状态，则不能退出守护程序
			if ( (S_RUN == pThis->m_bIsStoped && pThis->m_nExitCode == code) || pThis->m_bExit)
				break;
			pThis->Lock();
			pThis->m_handle = 0;
			pThis->mytick.reset();
			pThis->m_ThreadId = 0;
			pThis->Refresh();
			pThis->Unlock();
		}while(!pThis->m_bExit);
	}

	pThis->m_bExit = TRUE;
	pThis->m_bKeeeperExit = TRUE;
	pThis->SendMessage(WM_CLOSE, 0, 0);
	OutputDebugStringA("keepProc 线程已退出。\n");
	return 0xdead001;
}


// 检测假死
DWORD WINAPI CKeeperDlg::checkProc(LPVOID pParam)
{
	CKeeperDlg *pThis = (CKeeperDlg*)pParam;
	pThis->m_bCheckExit = FALSE;
	OutputDebugStringA("checkProc 线程已启动。\n");
	const char *strFileName = pThis->m_FileDescription;// 守护程序的描述
	while (!pThis->m_bExit)
	{
		HWND bFind = ::FindWindowA(NULL, strFileName);
		if (bFind)
		{
			pThis->ReStart(true);
			::SendMessage(bFind, WM_CLOSE, 0, 0);
		}
		Sleep(pThis->m_nWatchTime);
	}
	pThis->m_bCheckExit = TRUE;
	OutputDebugStringA("checkProc 线程已退出。\n");
	return 0xdead002;
}


// 远程通信
DWORD WINAPI CKeeperDlg::socketProc(LPVOID pParam)
{
	CKeeperDlg *pThis = (CKeeperDlg*)pParam;
	pThis->m_bSocketExit = FALSE;
	OutputDebugStringA("socketProc 线程已启动。\n");
	CBcecrSocket *pSock = pThis->m_pSocket;
	time_t tm = 0, resp = time(0);
	while (!pThis->m_bExit)
	{
		const char *ip = pThis->m_Ip;
		if (0 == *ip || 0 == pThis->m_nPort)
		{
			Sleep(50);
			continue;
		}
		if (ip[0] == '0' && ip[1] == 0)
			break;
		CServerFinder *finder = pThis->m_finder;
		if ( !pSock->IsConnected() && ((finder && finder->IsReady()) ? 
			pSock->init(CMyTask::GetSocket(), CMyTask::GetIp()) : pSock->init(ip, pThis->m_nPort, 1)) )
		{
			// 避免频繁连接远端IP
			int K = IS_NIGHT() ? 200 : 100;
			while(!pThis->m_bExit && --K) Sleep(50);
			resp = time(0);
			continue;
		}
		int n = 0;
		char buffer[SOCKET_BUF];
		if ( (n = pSock->recvData(buffer, sizeof(buffer)-1)) < 0 )
		{
			pSock->unInit();
			CMyTask::Reset();
			tm = 0;
			pThis->ffmpegStop("ffmpeg.exe");
			continue;
		}
		else if (n > 0)
		{
			buffer[n] = 0;
			bool flag = true; // 是否记日志
			char cmd[SOCKET_BUF];
			std::string arg = PARSE_CMD(buffer, cmd);
			resp = time(0);
			if (pSock->IsRegistered())
			{
				if (0 == strcmp(KEEPALIVE, cmd))
				{
					int n = atoi(arg.c_str());
					if (n != pThis->m_nAliveTime)
						pThis->m_nAliveTime = max(n, 1);
					else 
						flag = false;
				}
				else if (0 == strcmp(RESTART, cmd))
					pThis->ReStart(false);
				else if (0 == strcmp(REFRESH, cmd))
					pThis->Refresh();
				else if (0 == strcmp(STOP, cmd))
					pThis->Stop();
				else if (0 == strcmp(START, cmd))
					pThis->Start();
				else if (0 == strcmp(SHUTDOWN, cmd))
					pThis->Shutdown();
				else if (0 == strcmp(REBOOT, cmd))
					pThis->ReBoot();
				else if (0 == strcmp(SETTIME, cmd))
					pThis->SetTime(arg);
				else if (0 == strcmp(UPDATE, cmd))
					pThis->Update(arg);
				else if (0 == strcmp(NOTICE, cmd))
					pThis->Notice(arg.c_str());
				else if(0 == strcmp(PAUSE, cmd))
					pThis->m_bIsStoped = S_PAUSE;
				else if (0 == strcmp(ALLOW_DEBUG, cmd))
					theApp.is_debug = true;
				else if (0 == strcmp(WATCH, cmd))
					pThis->Watch(atoi(arg.c_str()));
				else if (0 == strcmp(SETPORT, cmd))
				{
					pThis->m_nPort = atoi(arg.c_str());
					pSock->unInit();
					CMyTask::Reset();
					tm = 0;
					pThis->ffmpegStop("ffmpeg.exe");
					pThis->InitRemoteIp(pThis->m_Ip, pThis->m_nPort);
					WritePrivateProfileStringA("settings", "port", arg.c_str(), pThis->m_strConf.c_str());
				}
				else if (0 == strcmp(SETIP, cmd))
				{
					strcpy_s(pThis->m_Ip, arg.c_str());
					pSock->unInit();
					CMyTask::Reset();
					tm = 0;
					pThis->ffmpegStop("ffmpeg.exe");
					pThis->InitRemoteIp(pThis->m_Ip, pThis->m_nPort);
					WritePrivateProfileStringA("settings", "remote", arg.c_str(), pThis->m_strConf.c_str());
				}
				else 
					flag = false;
			}else
			{
				if (0 == strcmp(REGISTER, cmd))
				{
					bool r = 0 == strcmp(arg.c_str(), "success");
					pSock->Register(r);
					if (r) pThis->Refresh();
				}
				else 
					flag = false;
			}
			if (flag)
				pThis->log_command(&resp, pSock->m_chToIp, cmd, arg.c_str());
		}else Sleep(10); //n==0
		// 心跳
		time_t cur = time(0);
		if (cur - tm >= pThis->m_nAliveTime)
		{
			tm = pSock->IsConnected() ? cur : 0;
			pThis->Refresh();
		}
		// 三个心跳无回复
		n = max(pThis->m_nAliveTime, 5);
		if (cur - resp >= (3*n))
		{
			resp = cur;
			pSock->unInit();
			CMyTask::Reset();
			pThis->ffmpegStop("ffmpeg.exe");
		}
	}
	pThis->m_bSocketExit = TRUE;
	OutputDebugStringA("socketProc 线程已退出。\n");
	return 0xdead003;
}


void CKeeperDlg::ReStartSelf(const CString &Path)
{
	theApp.SetReStart();
}


// 退出守护程序(bExitDst - 是否退出被守护程序)
void CKeeperDlg::ExitKeeper(bool bExitDst)
{
	m_bExit = TRUE;
	HWND hWnd = FindKeepWindow();
	if (bExitDst)
	{
		StopApp(false);
	}
	SetEvent(m_event);
	AddCloseMunu(hWnd);
}


void CKeeperDlg::OnOK()
{
	if (FALSE == m_bExit)
	{
		m_bExit = TRUE;
		SetEvent(m_event);
		HWND hWnd = FindKeepWindow();
		AddCloseMunu(hWnd);
		if (IDOK == MessageBox(_T("即将退出守护程序。需要退出被守护程序吗?"), 
			_T("警告"), MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2))
		{
			StopApp(false);
		}
	}
	CDialog::OnOK();
}

void CKeeperDlg::OnCancel()
{
	if (FALSE == m_bExit)
	{
		m_bExit = TRUE;
		SetEvent(m_event);
		HWND hWnd = FindKeepWindow();
		AddCloseMunu(hWnd);
		if (IDOK == MessageBox(_T("即将退出守护程序。需要退出被守护程序吗?"), 
			_T("警告"), MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2))
		{
			StopApp(false);
		}
	}
	CDialog::OnCancel();
}


BOOL CKeeperDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN)
	{
		return TRUE;
	}

	return CDialog::PreTranslateMessage(pMsg);
}


HWND CKeeperDlg::FindKeepWindow() const
{
	for (int i = 0; i < MAX_NAMES; ++i)
	{
		HWND hWnd = ::FindWindow(NULL, CString(m_sWindowNames[i]));
		if (hWnd)
			return hWnd;
	}
	return NULL;
}


void CKeeperDlg::OnBnClickedButtonShowconsole()
{
	HWND hWnd = FindKeepWindow();
	if (hWnd)
	{
		BOOL bShow = ::IsWindowVisible(hWnd);
		::ShowWindow(hWnd, bShow ? SW_HIDE : SW_SHOW);
		DeleteCloseMenu(hWnd);
		::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
		::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
	} else if (S_RUN == m_bIsStoped)
	{
		strcpy_s(m_sTitle, m_modulePath);// 尝试用模块路径恢复窗口标题
		InitTitles(m_sTitle);
		if (hWnd = FindKeepWindow())
		{
			::ShowWindow(hWnd, ::IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
			DeleteCloseMenu(hWnd);
			::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
			::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
			WritePrivateProfileStringA("settings", "title", m_sTitle, m_strConf.c_str());
		}else {
			Afx_MessageBox box(_T("根据程序的窗口标题未识别到窗口句柄。你应该通过\"托盘->右键->设置\"为它设置标题。"));
			box.DoModal();
		}
	}
}


void CKeeperDlg::OnAppAbout()
{
	CAboutDlg dlg;
	dlg.DoModal();
}


void CKeeperDlg::OnSelfStart()
{
	char pRegName[64] = { 0 };// 注册表项目名称
	sprintf(pRegName, "Keep_%s", m_moduleId);
	m_bAutoRun = !m_bAutoRun;
	if(FALSE == SetSelfStart(m_pKeeperPath, pRegName, m_bAutoRun))
	{
		MessageBox(CString(m_pKeeperPath) + _T("\r\n操作失败!"), _T("错误"), MB_ICONERROR);
	}
	CString csPath(m_pKeeperPath);
	int pos = csPath.ReverseFind('\\');
	CString sDir = csPath.Left(pos);
	CString conf_path(m_strConf.c_str());
	WritePrivateProfileString(_T("settings"), _T("auto_run"), m_bAutoRun ? L"1" : L"0", conf_path);
}


void CKeeperDlg::OnUpdateSelfStart(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bAutoRun);
}


void CKeeperDlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
	CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

	CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

	ASSERT(pPopupMenu != NULL);
	// Check the enabled state of various menu items.

	CCmdUI state;
	state.m_pMenu = pPopupMenu;
	ASSERT(state.m_pOther == NULL);
	ASSERT(state.m_pParentMenu == NULL);

	// Determine if menu is popup in top-level menu and set m_pOther to
	// it if so (m_pParentMenu == NULL indicates that it is secondary popup).
	HMENU hParentMenu;
	if (AfxGetThreadState()->m_hTrackingMenu == pPopupMenu->m_hMenu)
		state.m_pParentMenu = pPopupMenu;    // Parent == child for tracking popup.
	else if ((hParentMenu = ::GetMenu(m_hWnd)) != NULL)
	{
		CWnd* pParent = this;
		// Child windows don't have menus--need to go to the top!
		if (pParent != NULL &&
			(hParentMenu = ::GetMenu(pParent->m_hWnd)) != NULL)
		{
			int nIndexMax = ::GetMenuItemCount(hParentMenu);
			for (int nIndex = 0; nIndex < nIndexMax; nIndex++)
			{
				if (::GetSubMenu(hParentMenu, nIndex) == pPopupMenu->m_hMenu)
				{
					// When popup is found, m_pParentMenu is containing menu.
					state.m_pParentMenu = CMenu::FromHandle(hParentMenu);
					break;
				}
			}
		}
	}

	state.m_nIndexMax = pPopupMenu->GetMenuItemCount();
	for (state.m_nIndex = 0; state.m_nIndex < state.m_nIndexMax;
		state.m_nIndex++)
	{
		state.m_nID = pPopupMenu->GetMenuItemID(state.m_nIndex);
		if (state.m_nID == 0)
			continue; // Menu separator or invalid cmd - ignore it.

		ASSERT(state.m_pOther == NULL);
		ASSERT(state.m_pMenu != NULL);
		if (state.m_nID == (UINT)-1)
		{
			// Possibly a popup menu, route to first item of that popup.
			state.m_pSubMenu = pPopupMenu->GetSubMenu(state.m_nIndex);
			if (state.m_pSubMenu == NULL ||
				(state.m_nID = state.m_pSubMenu->GetMenuItemID(0)) == 0 ||
				state.m_nID == (UINT)-1)
			{
				continue;       // First item of popup can't be routed to.
			}
			state.DoUpdate(this, TRUE);   // Popups are never auto disabled.
		}
		else
		{
			// Normal menu item.
			// Auto enable/disable if frame window has m_bAutoMenuEnable
			// set and command is _not_ a system command.
			state.m_pSubMenu = NULL;
			state.DoUpdate(this, FALSE);
		}

		// Adjust for menu deletions and additions.
		UINT nCount = pPopupMenu->GetMenuItemCount();
		if (nCount < state.m_nIndexMax)
		{
			state.m_nIndex -= (state.m_nIndexMax - nCount);
			while (state.m_nIndex < nCount &&
				pPopupMenu->GetMenuItemID(state.m_nIndex) == state.m_nID)
			{
				state.m_nIndex++;
			}
		}
		state.m_nIndexMax = nCount;
	}
}


// 启动时隐藏对话框
void CKeeperDlg::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
	if (lpwndpos->flags & SWP_SHOWWINDOW)
	{
		lpwndpos->flags &= ~SWP_SHOWWINDOW;
		PostMessage(WM_WINDOWPOSCHANGING, 0, (LPARAM)lpwndpos);
		ShowWindow(SW_HIDE);
	}
	else
	{
		CDialog::OnWindowPosChanging(lpwndpos);
	}
}


void WritePrivateProfileInt(LPCSTR lpAppName, LPCSTR lpKeyName, int lpString, LPCSTR lpFileName)
{
	char buffer[128];
	sprintf_s(buffer, "%d", lpString);
	WritePrivateProfileStringA(lpAppName, lpKeyName, buffer, lpFileName);
}


// 配置守护程序的参数
void CKeeperDlg::OnSettings()
{
	CKeeperSettings dlg(m_strConf, this);
	CString ip = CString(m_Ip), title = CString(m_sTitle);

	dlg.m_nWatchTime = m_nWatchTime;
	dlg.m_nCpu = m_nAffinityCpu;
	dlg.m_nVisible = m_nVisible;
	dlg.m_strRemoteIp = ip;
	dlg.m_nRemotePort = m_nPort;
	dlg.m_strTitle = title;
	dlg.m_strIcon = m_strIcon;

	if (IDOK == dlg.DoModal())
	{
		bool bMod = false;
#define ASSIGN_s(a, b) if(a != b) { bMod = true; a = b; }
		
		ASSIGN_s(m_nWatchTime, dlg.m_nWatchTime);
		ASSIGN_s(m_nAffinityCpu, dlg.m_nCpu);
		ASSIGN_s(m_nVisible, dlg.m_nVisible);
		dlg.m_strRemoteIp = (L"未配置" == dlg.m_strRemoteIp ? L"" : dlg.m_strRemoteIp);
		// 远端信息修改后需要重新发起连接
		if (ip != dlg.m_strRemoteIp || m_nPort != dlg.m_nRemotePort)
		{
			bMod = true;
			ip = dlg.m_strRemoteIp;
			m_nPort = dlg.m_nRemotePort;
			if (m_pSocket)
			{
				m_pSocket->unInit();
				if(ip == CString("0"))
				{
					Afx_MessageBox dlg(_T("即将停止搜索远程设备! 除非手动编辑配置文件, 否则该功能不再开启!"), 8000);
					dlg.DoModal();
					if (m_finder)
						m_finder->Exit();
				}
			}
		}
		USES_CONVERSION;
		const char *s_ip = W2A(ip);
		if (title != dlg.m_strTitle) // 标题
		{
			bMod = true;
			title = dlg.m_strTitle;
			const char *s_title = W2A(title);
			if(title.GetLength() <= _MAX_PATH)
			{
				strcpy(m_sTitle, s_title);
				InitTitles(s_title);
			}
		}
		if (m_strIcon != dlg.m_strIcon) // 图标
		{
			bMod = true;
			m_strIcon = dlg.m_strIcon;
			SetDlgIcon(m_strIcon);
		}
		if(ip.GetLength() <= 64)
		{
			strcpy(m_Ip, s_ip);
			CServerFinder::SetWaitTime(5);
		}
		if (bMod)
		{
			WritePrivateProfileInt("settings", "watch_time", m_nWatchTime, m_strConf.c_str());
			WritePrivateProfileInt("settings", "visible", m_nVisible, m_strConf.c_str());
			WritePrivateProfileInt("settings", "cpu", m_nAffinityCpu, m_strConf.c_str());
			WritePrivateProfileStringA("settings", "title", m_sTitle, m_strConf.c_str());
			WritePrivateProfileStringA("settings", "icon", W2A(m_strIcon), m_strConf.c_str());
			WritePrivateProfileStringA("settings", "remote", m_Ip, m_strConf.c_str());
			WritePrivateProfileInt("settings", "port", m_nPort, m_strConf.c_str());
			InitRemoteIp(m_Ip, m_nPort);
		}
	}
}

// 定期记录被守护程序的状态
void CKeeperDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (1 == nIDEvent && m_handle)
	{
		time_t TM = time(NULL);
		char content[256];
		int n = GetProcessId(m_handle);
		sprintf_s(content, "进程 %d, 内存 %.2fM, 线程 %d, 句柄 %d.", n,
			m_cpu.get_mem_usage(), 
			GetThreadCount(n),
			m_cpu.get_handles_count());
		log_command(&TM, content);
	}
	CDialog::OnTimer(nIDEvent);
}
