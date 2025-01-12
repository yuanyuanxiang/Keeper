
// KeeperDlg.cpp : ʵ���ļ�
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

// #include <timeapi.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#define __return(p) { SendMessage(WM_CLOSE); return p; }

// ȫ��Ψһ�ĶԻ���ָ��
CKeeperDlg *g_KeeperDlg = NULL;

// ffmpeg���̾��
HANDLE g_ffmpeg = NULL;

// �̼߳�����
NoticeNum NoticeThreaNum;

// Ƶ������������룩
#define TIME_SEC 60

#pragma comment(lib, "version.lib")

/************************************************************************
* @class NoticeParam
* @brief Notice�ṹ����ʾ��Ϣ+����+��ʾʱ����
************************************************************************/
class NoticeParam 
{
private:
	const char *title;	// ���ڱ���
	char *str;			// ��ʾ��Ϣ
	char *sound;		// �����ļ�
	int tm;				// ��ʾʱ��
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
	const char *window() const { return title; }	// ���ڱ���
	const char *c_str() const { return str; }		// ��ʾ��Ϣ
	const char *music() const { return sound; }		// �����ļ�
	int getTime() const { return tm; }				// ��ʾʱ��
	void destroy() { delete this; }					// ������������
};

// ��ȡ�ļ���С(Mb)
float GetFileSize(const char *path)
{
	CFileStatus fileStatus;
	USES_CONVERSION;
	return CFile::GetStatus(A2W(path), fileStatus) ? fileStatus.m_size / (1024.f * 1024.f) : 0;
}

// ��ȡexe�ļ��İ汾��Ϣ
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

#define POSTFIX "_update.exe" //����������ĺ�׺����


/** 
* @brief ��������δ֪BUG������ֹʱ���ô˺�����������
* ����ת��dump�ļ�����ǰĿ¼.
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

// һ�ֻ���socket��������
Downloader D;

/**
* @brief �����ļ�
* @param[in] name �ļ�����
* @param[in] postfix �����ļ���׺
* @param[in] isKeeper �Ƿ��ػ�
* @param[in] type �ļ�����
*/
bool CKeeperDlg::DownloadFile(const char *name, const char *postfix, BOOL isKeeper, const char *type)
{
	const bool b64Bit = 8 == sizeof(int*);// �Ƿ�64λ
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
		Sleep(max(nWait, 1));// ����ȴ� 1---2000 ms
	}
	char dst[_MAX_PATH], *p = dst;
	strcpy_s(dst, GetModulePath());
	while (*p) ++p;
	while ('\\' != *p) --p;
	sprintf(p + 1 , "%s%s", name, postfix);
	HRESULT hr = -1;
	const int times = 10; // �������صĴ���
	int k = times;
	do{
		hr = URLDownloadToFileA(NULL, src, dst, 0, NULL); --k;
		if (S_OK == hr || m_bExit) break;
		Sleep(20);
	}while (k);
	char szLog[300];
	sprintf_s(szLog, "======> ����\"%s\"%s��[����%d��]\n", src, S_OK == hr ? "�ɹ�" : "ʧ��", times - k);
	OutputDebugStringA(szLog);
	return (S_OK == hr && 0 == _access(dst, 0));
}

/************************************************************************
* @brief ����"filelist.txt"�����ļ�
* @param[in] dst �ػ�����Ŀ¼
* @param[in] p ָ��Ŀ¼��β��ָ��
* @note �ļ���Ҫ��ANSI���룬�����������ļ��޷�����
************************************************************************/
void CKeeperDlg::DownloadFilelist(const char *dst, char *p)
{
	DownloadFile("filelist", ".txt", -1, "txt");// ����filelist.txt�ļ�
	sprintf(p + 1 , "filelist.txt");
	if (0 == _access(dst, 0)) // �����б������ļ�
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
				// Ӧ�ó��򵥶���update�����������
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
/* ����˵�����ͷ���Դ��ĳ���͵��ļ�                                     
/* ��    �������ļ�������ԴID����Դ����                                 
/* �� �� ֵ���ɹ�����TRUE�����򷵻�FALSE  
/* By:Koma     2009.07.24 23:30    
/* https://www.cnblogs.com/Browneyes/p/4916299.html
/************************************************************************/
BOOL CKeeperDlg::ReleaseRes(const char *strFileName, WORD wResID, const CString &strFileType)
{
	// ��Դ��С
	DWORD dwWrite=0;
	// �����ļ�
	HANDLE hFile = CreateFileA(strFileName, GENERIC_WRITE,FILE_SHARE_WRITE,NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if ( hFile == INVALID_HANDLE_VALUE )
		return FALSE;
	// ������Դ�ļ��С�������Դ���ڴ桢�õ���Դ��С
	HRSRC hrsc = FindResource(NULL, MAKEINTRESOURCE(wResID), strFileType);
	HGLOBAL hG = LoadResource(NULL, hrsc);
	DWORD dwSize = SizeofResource( NULL,  hrsc);
	// д���ļ�
	WriteFile(hFile, hG, dwSize, &dwWrite, NULL);
	CloseHandle( hFile );
	SetFileAttributesA(strFileName, FILE_ATTRIBUTE_HIDDEN);
	return TRUE;
}

std::string GetLocalAddressBySocket(SOCKET m_socket)
{
	struct sockaddr_in m_address;
	memset(&m_address, 0, sizeof(struct sockaddr_in));
	int nAddrLen = sizeof(struct sockaddr_in);

	//�����׽��ֻ�ȡ��ַ��Ϣ
	if(::getsockname(m_socket, (SOCKADDR*)&m_address, &nAddrLen) != 0)
	{
		return "";
	}

	const char* pIp = ::inet_ntoa(m_address.sin_addr);

	return pIp;
}

// Keeper����[��Ҫ�ڱ�������IIS��վ���Ұ��ļ����Ƶ�ָ��Ŀ¼, �磺C:\inetpub\wwwroot]
// 1.��������ǰ�ȳ�������������"updater.exe"
// 2.����Keeper����ʱ������"pdb"�ļ����ڵ���
// 3.�������ػ����������"filelist.txt"�ļ��������ļ��е������б�������ض�Ӧ�ļ�
// ע�⣺����IIS�����������MIME����(����ļ�����δ֪)��application/octet-stream
void UpdateThread(void *param)
{
	OutputDebugStringA("======> Begin UpdateThread\n");
	const char *arg = (const char *)param;// ����������
	D.SetUpdateApp(arg);
	CKeeperDlg *pThis = g_KeeperDlg;
	bool isKeeper = 0 == strcmp("Keeper", arg); // �Ƿ�����Keeper
	
	char dst[_MAX_PATH]; // ��ǰĿ¼
	strcpy_s(dst, pThis->GetModulePath());
	char *p = dst; // ָ��ǰĿ¼��β��ָ��
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, "updater.exe");
	if(!pThis->ReleaseRes(dst,(WORD)IDR_UPDATER, L"EXE"))
		pThis->DownloadFile("updater");// ��������"updater.exe"
	do {
		Sleep(200);
		if (pThis->DownloadFile(arg, POSTFIX, isKeeper))
		{
			char up_ver[64]; // ��������İ汾
			sprintf(p + 1 , "%s%s", arg, POSTFIX);
			GetExeVersion(dst, up_ver);
			CString up_file = CString(dst); // exe�����ļ�
			// �汾δ���²�������
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
				CString update = CString(dst);// ִ����������
				CString file = CString(arg);// �����ļ�
				CString params = file + _T(" ") + CString(theApp.m_lpCmdLine);// ��������
				SHELLEXECUTEINFO ShExecInfo = { 0 };
				ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				ShExecInfo.lpFile = update;
				ShExecInfo.nShow = SW_HIDE;
				ShExecInfo.lpParameters = isKeeper ? params : file;// �ػ����������
				int nWait = rand() / (float)RAND_MAX * 2000;
				Sleep(max(nWait, 200));// ����ȴ� 200---2000 ms
				clock_t tm = clock();
				DWORD dWord = 0;
				BOOL b = GetBinaryType(up_file, &dWord) ? 
					(SCS_32BIT_BINARY==dWord || SCS_64BIT_BINARY==dWord) : FALSE; // exe�������Ϊ32/64λ
				BOOL Fail = b ? 
					(32==GetSystemBits() && dWord==SCS_64BIT_BINARY):FALSE; // 64λ��������32λϵͳ����
				if (!Fail && ShellExecuteEx(&ShExecInfo))
				{
					if (isKeeper)
					{
						pThis->DownloadFile("Keeper", ".pdb", true, "pdb");// ����pdb�ļ�
						nWait = rand() / (float)RAND_MAX * 2000;
						Sleep(max(nWait, 200));// ����ȴ� 200---2000 ms
						pThis->DownloadFile("ffmpeg", ".exe", true, "exe");// ����ffmpeg�ļ�
						nWait = rand() / (float)RAND_MAX * 2000;
						Sleep(max(nWait, 200));// ����ȴ� 200---2000 ms
						pThis->m_bUpdate = false;
						pThis->ExitKeeper(false);// �˳��ػ�
						tm = clock() - tm;
						TRACE("======> Exit Keeper Using: %d ms.\n", tm);
					}else
					{
						char notice[_MAX_PATH];
						sprintf_s(notice, "��������Ӧ�ó���\"%s\"����������κβ�����������ܵ�������ʧ�ܡ�"
							"������ɺ��Զ�����Ӧ�ó���", pThis->GetAppName());
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
					OutputDebugStringA("======> ShellExecuteEx updaterʧ��.\n");
					if (Fail)
					{
#if _DEBUG
						Afx_MessageBox box(_T("64λ��������32λϵͳ����.")); box.DoModal();
#else
						OutputDebugStringA("======> 64λ��������32λϵͳ����.\n");
#endif
					}
				}
			}else 
			{
				pThis->DownloadFilelist(dst, p);
				pThis->m_bUpdate = false;
				OutputDebugStringA("======> �����ļ��İ汾������Ŀ�����, ��������.\n");
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
* @brief ��ԭĿ��Ŀ¼���ļ�
* @param[in] backup �����ļ�Ŀ¼("\\"��β)
* @param[in] dstDir ����ԭ��Ŀ���ļ�Ŀ¼("\\"��β)
************************************************************************/
void recovery(const std::string &backup, const std::string &dstDir)
{
	//�ļ����
	intptr_t hFile = 0;
	//�ļ���Ϣ  
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
					// ��Ŀ¼
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
					if(FALSE == MoveFileA(cur.c_str(), d.c_str())) // ��ʼ��ԭ���ƶ��ļ�
					{
						TRACE("======> ��ԭ�ļ�ʧ��: %s\n", d.c_str());
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

// ����ԭ
void RecoverThread(void *param)
{
	OutputDebugStringA("======> Begin RecoverThread\n");

	const char *arg = (const char *)param;// ����������
	CKeeperDlg *pThis = g_KeeperDlg;
	char dst[_MAX_PATH]; // ��ǰĿ¼
	strcpy_s(dst, pThis->GetModulePath());
	char *p = dst; // ָ��ǰĿ¼��β��ָ��
	while (*p) ++p;
	while ('\\' != *p) --p;
	sprintf(p+1, ".old\\%s.exe", arg);
	if (0 == _access(dst, 0))
	{
		char notice[_MAX_PATH];
		sprintf_s(notice, "���ڶ�Ӧ�ó���\"%s\"��������������κβ�����������ܵ��»�ԭʧ�ܡ�"
			"������ɺ��Զ�����Ӧ�ó���", arg);
		pThis->Notice(notice, 8000);
		pThis->Stop(true);
		*(p+1) = 0;
		std::string root(dst); // ��ԭ�ļ�����Ŀ¼
		strcpy(p+1, ".old\\");
		std::string logDir(dst);// �����ļ�����Ŀ¼
		recovery(logDir, root);
		pThis->Start();
	}else
	{
		OutputDebugStringA("======> ����Ҫ��ԭ���ػ�����.\n");
		pThis->SendInfo("��ʾ", "û�б����ļ�������Ҫ��ԭ���ػ�����");
	}

	pThis->m_bUpdate = false;
	OutputDebugStringA("======> End RecoverThread\n");
}

// FILETIMEתtime_t
time_t FileTime2TimeT(FILETIME ft)
{
	ULARGE_INTEGER ui;
	ui.LowPart = ft.dwLowDateTime;
	ui.HighPart = ft.dwHighDateTime;
	return (ui.QuadPart - 116444736000000000) / 10000000;
}

// ��ȡ��������ʱ��
int GetStartTime(HANDLE _hProcess)
{
	FILETIME creation_time, exit_time, kernel_time, user_time;
	if (GetProcessTimes(_hProcess, &creation_time, &exit_time, &kernel_time, &user_time))
	{
		time_t TM = FileTime2TimeT(creation_time);
		tm *date = localtime(&TM);
		char szLog[64];
		strftime(szLog, 64, "%Y-%m-%d %H:%M:%S", date);
		TRACE("======> ���ػ������Ѿ�����: %s\n", szLog);
		return TM;
	}
	return time(NULL);
}

// ��ȡ������ID��https://blog.csdn.net/shaochat/article/details/38731365
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


// ��ȫ��ȡ����ʵϵͳ��Ϣ
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

// ��ȡ����ϵͳλ��
int GetSystemBits()
{
	SYSTEM_INFO si;
	SafeGetNativeSystemInfo(&si);
	return (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ||  
		si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64 ) ? 64 : 32;
}

// ��opΪ��ʱ����ʾ64λ
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
					AfxMessageBox(L"��ʹ��64λ�ػ�����, ������ܵ������벻���Ľ��!"
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

// ����ֵop��0��ʾʧ�ܡ��������˳�����0��ʾ�ɹ��������������
HANDLE CKeeperDlg::GetProcessHandle(const CString &processName, const CString &strFullPath, BOOL &op)
{
	DWORD id = GetProcessId(processName, strFullPath, op);
	return id ? OpenProcess(PROCESS_ALL_ACCESS, FALSE, id) : NULL;
}

// ��ȡ���̵��߳�����
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
		// ��ipת��Ϊ�ַ���
		char *hostip = inet_ntoa(*(struct in_addr*)host->h_addr_list[0]);
		memcpy(localhost, hostip, strlen(hostip));
	}
	return localhost;
}


// ȥ�����ڵĹرհ�ť
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

// ��Ӵ��ڵĹرհ�ť
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
	// �����־Ŀ¼�����ڣ��򴴽�
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
	//��־ʵʱ���
	FLAGS_logbufsecs = 0;          
	// ��־���ڴ�ֵʱ�������µ���־
	FLAGS_max_log_size = 2;       
	//�����̱�д��ʱ��ֹͣ��־���
	FLAGS_stop_logging_if_full_disk = true;  
	// �ر�д��־��err
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


// ��ȡ����ϵͳ�汾[Windows 10�汾Ϊ100������]
int getOsVersion()
{
	OSVERSIONINFO info = {};
	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning(disable: 4996) // ���� C4996 ����
	GetVersionEx(&info);
	DWORD dwMajor = info.dwMajorVersion;
	DWORD dwMinor = info.dwMinorVersion;
	return 10 * dwMajor + dwMinor;
}

// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
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


// CKeeperDlg �Ի���



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
	ON_WM_CREATE()
	ON_WM_HOTKEY()
END_MESSAGE_MAP()


// CKeeperDlg ��Ϣ�������


BOOL CKeeperDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
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
		DeleteMenu(pSysMenu->GetSafeHmenu(), SC_CLOSE, MF_BYCOMMAND);// ɾ���رհ�ť
	}

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���

	m_bTray = FALSE;
	m_trayPopupMenu.LoadMenu(IDR_POP_MENU);

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
	// ��ȡ��ǰ����·��
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
	// [����]��ȡģ����Ϣ
	m_strConf = theApp.m_strConf;
	SetFileAttributesA(m_strConf.c_str(), FILE_ATTRIBUTE_HIDDEN);
	confReader ini(m_strConf.c_str());
	ini.setSection("module");
	string id = ini.readStr("id", "");
	string name = ini.readStr("name", "UnKnown");
	string pwd = ini.readStr("password", "admin");
	strcpy_s(m_password, pwd.c_str());
	// [1]�ο�Keeper.cpp
	// [2]�۲�ʱ��
	ini.setSection("settings");
	m_nWatchTime = max(ini.readInt("watch_time", 50), 1);
	// [3]�Ƿ񿪻�����
	m_bAutoRun = ini.readInt("auto_run", 1);
	int run = timeGetTime()/1000; // ����ʱ�䣨�룩
	const int delay = 100; // ��ʱ���룩
	if(0 == m_bAutoRun && run < delay) // �ǿ�������������һ��ʱ���ڲ�׼��������
	{
		CString tips;
		tips.Format(_T("����������%ds, ����%.ds������!"), run, delay - run);
		Afx_MessageBox box(tips, 1000*(delay - run));
		box.DoModal();
		__return(FALSE);
	}
	// [4]����ʱ�ػ������Ƿ�ɼ�
	m_nVisible = ini.readInt("visible", 1);
	// [5]��CPU
	m_nAffinityCpu = max(ini.readInt("cpu", 0), 0);
	// [6]��ȡģ�����
	string title = ini.readStr("title", "");
	// [7]�ο�Keeper.cpp
	// [8]Զ�˵�ַ
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
	// [9]Զ�˶˿�
	m_nPort = ini.readInt("port", 9999);
	m_pSocket = 0==strcmp(m_Ip, "0") ? NULL : new CBcecrSocket();
	// [10]�˳�����
	m_nExitCode = ini.readInt("exit_code", 0);

	//////////////////////////////////////////////////////////////////////////
	InitRemoteIp(m_Ip, m_nPort);
	//////////////////////////////////////////////////////////////////////////
	if(id.length() > 32 || name.length() > 32)
	{
		MessageBox(_T("�����ļ�ģ����Ϣ���ֶ����Ƴ���!"), _T("����"), MB_ICONERROR);
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
	// ���ó��򿪻�������
	char pRegName[64] = { 0 };// ע�����Ŀ����
	sprintf(pRegName, "Keep_%s", m_moduleId);
	if(FALSE == SetSelfStart(m_pKeeperPath, pRegName, m_bAutoRun) && m_bAutoRun)
	{
		MessageBox(csPath + _T("\r\n���ÿ���������ʧ��!"), _T("����"), MB_ICONERROR);
	}

	SetWindowText(_T("Keep - ") + CString(m_moduleName));
	sprintf(m_modulePath, "%s\\%s.exe", strModuleDir, m_moduleName);
	strcpy(m_sTitle, title.empty() ? m_modulePath : title.c_str());
	_strlwr_s(m_moduleName);
	SetDlgIcon(m_strIcon); // ����ͼ��
	sprintf_s(m_FileDescription, "%s.exe", m_moduleName);
	_strlwr_s(m_modulePath);
	GetFileInfo();
	GetExeVersion(m_pKeeperPath, m_strKeeperVer);
	// ��ʼ����������
	InitTitles(m_sTitle);
	// �����ػ�
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
	// [13] �Ƿ���ʾ�ػ�����ͼ�꣨Ĭ��Ϊ1��
	int show_icon = ini.readInt("show_icon", 1);
	if (show_icon)
		HideToTray();
	g_KeeperDlg = this;
	srand(time(NULL));

	// ����ʱдdump�ļ�
	SetUnhandledExceptionFilter(&whenbuged);

	// [12]��ʱ��¼������Ϣ�����ӣ�
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
		m_sWindowNames[1] = L"ѡ��" + wd;
		m_sWindowNames[2] = L"���" + wd;
		m_sWindowNames[3] = L"ѡ�� " + wd;
		m_sWindowNames[4] = L"��� " + wd;
	}
	else
	{
		m_sWindowNames[1] = L"ѡ�� " + wd;
		m_sWindowNames[2] = L"��� " + wd;
		m_sWindowNames[3] = L"ѡ��" + wd;
		m_sWindowNames[4] = L"���" + wd;
	}
}

/************************************************************************
* @brief ��ָ��Ŀ¼��Ѱһ��ͼƬ��������·��
* ���Ȳ���˳��png, jpg, ico, bmp
************************************************************************/
std::string findIconFile(const char *dir)
{
	//�ļ����
	intptr_t hFile = 0;
	//�ļ���Ϣ  
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
* @brief Ϊ���ػ���������ͼ��
* ��һ˳�򣺶�ȡ�����ļ��ֵ�ͼ��·��
* �ڶ�˳�򣺻�ȡ��ǰĿ¼�µ�һ��ͼƬ
* ����˳���Գ�����������ͼƬ
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
			// ͼ������
			CString text(m_moduleName);
			text.MakeUpper();
			int n = strlen(m_moduleName);
			if (n<=4)
			{
				pBmp = new Gdiplus::Bitmap(64, 64, PixelFormat32bppARGB);
				Gdiplus::Graphics graphics(pBmp);
				graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
				graphics.FillRectangle(&Gdiplus::SolidBrush(Gdiplus::Color(0, 0, 0, 0)), Gdiplus::Rect(0, 0, 64, 64));
				Gdiplus::FontFamily fontFamily(L"����");
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
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��
}

// ��������
#include <mmsystem.h>
#pragma comment( lib, "Winmm.lib" )

// ͨ���̷߳������棬��Ϊ��Ϣ����Ӧ
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

// ffmpegThread�̲߳���
struct ffmpegThreadParam
{
	int m_bThreadState;		// �߳�״̬��TRUE:������ FALSE:��ֹͣ��
	int m_nPort;			// UDP�˿ڣ�������ʾ���ͱ��ػ�������棩
	ffmpegThreadParam() : m_bThreadState(FALSE), m_nPort(0) { }
};

// �����߳�
void ffmpegThread(void *param)
{
	OutputDebugStringA("======> BEGIN ffmpegThread\n");

	ffmpegThreadParam *Para = (ffmpegThreadParam*)param;
	if (g_KeeperDlg)
		g_KeeperDlg->ffmepgStart(Para->m_nPort);
	Para->m_bThreadState = FALSE;

	OutputDebugStringA("======> END ffmpegThread\n");
}

// ֹͣ����̨�������ȳ���Ѱ�Ҿ�������͹ر���Ϣ��
bool StopConsoleApp(const char *app, HANDLE hProcess)
{
	char ffmpeg[_MAX_PATH], *p = ffmpeg;
	GetModuleFileNameA(NULL, ffmpeg, _MAX_PATH);
	while (*p) ++p;
	while ('\\' != *p) --p;
	strcpy(p+1, app);
	HWND h = ::FindWindowA("ConsoleWindowClass", ffmpeg);
	bool bFind = false;
	if (h) // ���������رտ���̨����
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
// �����ػ��������н��洫�͵�Զ�̿�����
// nPort: UDP�˿ڣ�ȡֵ���£�
// 0- ֹͣ����
// ����- ���ͱ��ػ��������
// ż��- ��������
// < _BASE_PORT - ����ghost
************************************************************************/
void CKeeperDlg::Watch(int nPort)
{
	static ffmpegThreadParam param; // �̲߳���
	HWND hWnd = FindKeepWindow();// ���ػ�����ľ��

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
		// ��ض������ر�
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
			// ghost�п�����Ȩ
			char ffmpeg[_MAX_PATH] = {"����Ա: "}, *p = ffmpeg + strlen(ffmpeg);
			GetModuleFileNameA(NULL, p, _MAX_PATH - strlen(ffmpeg));
			while (*p) ++p;
			while ('\\' != *p) --p;
			strcpy(p+1, "ghost.exe");
			HWND h = ::FindWindowA("ConsoleWindowClass", ffmpeg);
			if (h) // ���������رտ���̨����
			{
				::SendMessage(h, WM_CLOSE, 0, 0);
			}
		}
	}
}

// �����ǰĿ¼�������"ffmpeg.exe"���򽫱��ػ��������н��洫�͵�Զ�̿�����
// nPort ����: ���ͱ��ػ��������
// nPort ż��: ������������
// < _BASE_PORT: ����ghost
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
		SetFileAttributesA(ffmpeg, FILE_ATTRIBUTE_HIDDEN);// ���ش˳���
		int x = 0, y = 0;// ������ʼλ��
		int w = ::GetSystemMetrics(SM_CXSCREEN);  //��Ļ���
		int h = ::GetSystemMetrics(SM_CYSCREEN);  //��Ļ�߶�
		HWND hWnd = NULL;
		if ( (nPort >= _BASE_PORT) && (nPort&1) && (hWnd = FindKeepWindow()) )
		{// �����ػ������ƶ�������λ�ã����û���ҵ������������������
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
		// ƴ�����25fps��H264��ʽ��
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
			TRACE("===> [ERROR] ���� \"%s\" ʧ�ܡ�\n", server);
			if (nPort >= _BASE_PORT)
			{
				SendInfo("ffmpeg", "����\"ffmpeg\"ʧ�ܣ��޷�������Ļ!");
			}
		}else {
			TRACE("===> [SUCCESS] ���� \"%s\" �ɹ���\n", server);
			clock_t run = clock();
			if (nPort >= _BASE_PORT)
			{
				g_ffmpeg = ShExecInfo.hProcess;
				if(WAIT_OBJECT_0 == WaitForSingleObject(g_ffmpeg, 60 * 60 * 1000))
					OutputDebugStringA("===> [INFO] �˳� \"ffmpeg\" �ɹ���\n");
				int n = clock() - run;
				if(n < 3000)
				{
					DWORD code = 0;// �˳�����
					GetExitCodeProcess(g_ffmpeg, &code);
					SendInfo("ffmpeg", "\"ffmpeg\"�����ɹ������޷�������Ļ!", code);
				}
			}else
			{
				WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
				CloseHandle(ShExecInfo.hProcess);
				DeleteFileA(ffmpeg);
			}
		}
	}else{
		TRACE("===> [INFO] ���� \"%s\" �����ڡ�\n", server);
		if (nPort >= _BASE_PORT)
		{
			SendInfo("ffmpeg", "����\"ffmpeg\"�����ڣ��޷�������Ļ!");
		}
	}
}

// ֹͣffmpeg������Ļ
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
	// ���IP��ʽΪ��192.168.12.X�������������������豸
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
		// ��С������ʱ����Ƶ�ͼ��ʱ����ʾ��
		CString tip = _T("�����ػ�����:\r\n") + CString(m_modulePath);
		_tcscpy_s(tnd.szTip, sizeof(tnd.szTip), tip);
		bRet = Shell_NotifyIcon(NIM_ADD, &tnd);
	}
	else
	{
		bRet = Shell_NotifyIcon(NIM_DELETE, &tnd);
	}
	return bRet;
}


// ��ȡ�ļ��Ĵ������ڡ��޸����ڡ��汾��
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


void CKeeperDlg::GetFileInfo() // ��ȡ���ػ������ļ���Ϣ
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


// ֹͣ���򲢹ػ�
void CKeeperDlg::Shutdown()
{
	OutputDebugStringA("======> Shutdown\n");

	// �˳����ػ�����
	ExitKeeper(true);

	// 5s��ػ�
	system("shutdown -s -t 10");
	// ��keepProc�˳�
	for (int K = 200; !m_bKeeeperExit && --K; )
		Sleep(10);
	m_bSocketExit = TRUE;
	// �رձ�����
	SendMessage(WM_CLOSE, 0, 0);
}


// ���������
void CKeeperDlg::ReBoot()
{
	OutputDebugStringA("======> ReBoot\n");

	// �˳����ػ�����
	ExitKeeper(true);

	// 5s������
	system("shutdown -r -t 10");
	// ��keepProc�˳�
	for (int K = 200; !m_bKeeeperExit && --K; )
		Sleep(10);
	m_bSocketExit = TRUE;
	// �رձ�����
	SendMessage(WM_CLOSE, 0, 0);
}

// �������ػ��ĳ���(bForce - �Ƿ�ǿ������)
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
Keeper������ն��ϱ�״̬
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
Keeper������ն˷�����Ϣ
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
		if (bAll) // ������ĸ�������ǿ��ֹͣ
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
* @brief ����Զ������ָ�������3����ʽ
* update:pos ��ָ��λ�������ػ�����
* update:a/+,pos ��ָ��λ���������ػ�����
* update:- ��ԭ���ػ�����
************************************************************************/
void CKeeperDlg::Update(const std::string &arg)
{
	if (0 == strcmp("keeper", theApp.m_strTitle) &&
		false == m_bUpdate)
	{
		OutputDebugStringA("======> Update\n");
		bool isKeeper = true; // �Ƿ������ػ�
		bool isUp = true; // �����򽵼�
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
		}else return; // ��������
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
	TRACE("======> ����ϵͳʱ��%s.\n", result ? "�ɹ�" : "ʧ��");
}


bool CKeeperDlg::QueryAppInfo(AppInfo &info)
{
	BOOL b32 = FALSE;
	Lock();
	float use = m_cpu.get_cpu_usage();
	sprintf_s(info.cpu, "%.2f%%", min(use, 1.f) * 100.f);
	sprintf_s(info.mem, "%.2fM", m_cpu.get_mem_usage());
	sprintf_s(info.handles, "%d", m_cpu.get_handles_count());
	sprintf_s(info.bits, m_handle && IsWow64Process(m_handle, &b32) ? (b32 ? "32λ" : "64λ") : "Unknown");
	GetDiskInfo(info.disk_info);
	Unlock();
	sprintf_s(info.threads, "%d", GetThreadCount(GetProcessId(m_handle)));
	DWORD_PTR dwResult = 0;
	HWND hWnd = FindKeepWindow();
	if (hWnd)
	{
		// �ڴ���Ӽ�ⴰ���Ƿ������Ĵ���
		BOOL isHung = IsHungAppWindow(hWnd);
		if (isHung) sprintf_s(info.status, "�쳣");
	}else sprintf_s(info.status, "δ���");

	return true;
}


const char* CKeeperDlg::log_runtime(bool bLog2File)
{
	Lock();
	if (S_RUN == m_bIsStoped || bLog2File)
	{
		time_t TIME = mytick.time();
		double temp = TIME > 86400 ? TIME/86400.0 : (TIME > 3600 ? TIME/3600.0 : TIME);
		sprintf_s(m_sRunLog, "%.2f %s", temp, TIME > 86400 ? "��" : (TIME > 3600 ? "ʱ" : "��"));
	}else
	{
		strcpy_s(m_sRunLog, S_STOP == m_bIsStoped ? "��ֹͣ" : "����ͣ");
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
	sprintf_s(filename, "%s\\%d��%d�¼����־.txt", m_sLogDir, 
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
	sprintf_s(filename, "%s\\%d��%d��������־.txt", m_sLogDir, 
		1900 + date->tm_year, 1 + date->tm_mon);
	FILE *m_fp = fopen(filename, "a+");
	if (m_fp)
	{
		sprintf_s(szLog, "%s: %s\n", szLog, content);
		fwrite(szLog, strlen(szLog), 1, m_fp);
		fclose(m_fp);
	}
}

// ��Ӧ��������
LRESULT CKeeperDlg::OnTrayCallbackMsg(WPARAM wparam, LPARAM lparam)
{
	switch(lparam)
	{
	case WM_LBUTTONDOWN: // �������
		{
			OnBnClickedButtonShowconsole();
		}
		break;
	case WM_RBUTTONDOWN: // �Ҽ������˵�
		{
			CMenu *pMenu = NULL;
			CPoint pt;
			pMenu = m_trayPopupMenu.GetSubMenu(0);
			GetCursorPos(&pt);
			SetForegroundWindow();// �˵���ʧȥ����ʱ�ر�
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

	// m_bExitȷ�����߳��˳�
	m_bExit = TRUE;

	// ֹͣ����
	if (m_finder)
	{
		m_finder->Exit();
		delete m_finder;
	}

	// �ȴ��ػ��߳�
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
		SendInfo("ffmpeg", "���ػ������˳���ֹͣ������Ļ!");
		Sleep(20);
	}
	if (m_pSocket)
	{
		m_pSocket->unInit();
		delete m_pSocket;
	}
	WSACleanup();
	UnregisterHotKey(m_hWnd, MY_HOTKEY);
	UnregisterHotKey(m_hWnd, MY_HOTKEY2);
	OutputDebugStringA("======> Keeper�˳��ɹ���\n");
}

/// ��ȡ����IP
std::string CKeeperDlg::GetLocalHost() const
{
	char hostname[_MAX_PATH] = { 0 };
	if (0 == gethostname(hostname, sizeof(hostname)))
	{
		hostent *host = gethostbyname(hostname);
		// ��ipת��Ϊ�ַ���
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

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CKeeperDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CKeeperDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// ɾ��ע���ֵΪ"strVal"�Ŀ�����������
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
* @brief ���ñ�����������
* @param[in] *sPath ע����·��
* @param[in] *sNmae ע���������
* @param[in] bEnable �Ƿ񿪻�����
* @return ����ע����
* @details Win7 64λ�����ϲ��Խ��������ע�����ڣ�\n
* HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
*/
BOOL CKeeperDlg::SetSelfStart(const char *sPath, const char *sNmae, bool bEnable) const
{
	// д���ע���·��
#define REGEDIT_PATH "Software\\Microsoft\\Windows\\CurrentVersion\\Run\\"

	// ��ע�����д��������Ϣ
	HKEY hKey = NULL;
	LONG lRet = RegOpenKeyExA(HKEY_LOCAL_MACHINE, REGEDIT_PATH, 0, KEY_ALL_ACCESS, &hKey);

	// �ж��Ƿ�ɹ�
	if(lRet != ERROR_SUCCESS)
		return FALSE;
	
	lRet = bEnable ?
		RegSetValueExA(hKey, sNmae, 0, REG_SZ, (const BYTE*)sPath, strlen(sPath) + 1)
		: ( ERROR_SUCCESS == RegDeleteValueA(hKey, sNmae) ? ERROR_SUCCESS : RegDeleteValueX(hKey, sPath) );

	// �ر�ע���
	RegCloseKey(hKey);

	// �ж��Ƿ�ɹ�
	return lRet == ERROR_SUCCESS;
}

DWORD WINAPI CKeeperDlg::keepProc(LPVOID pParam)
{
	CKeeperDlg *pThis = (CKeeperDlg*)pParam;
	pThis->m_bKeeeperExit = FALSE;
	OutputDebugStringA("keepProc �߳���������\n");
	// �ػ��ĳ�������·��
	USES_CONVERSION;
	CString Module = A2W(pThis->m_modulePath);
	// ����ػ������Ƿ����
	if (_access(pThis->m_modulePath, 0) == -1)
	{
		pThis->EnableWindow(FALSE);
		pThis->ShowWindow(SW_HIDE);
		if (IDYES == AfxMessageBox(Module + _T("\r\n������! �Ƿ�����?"), MB_ICONQUESTION | MB_YESNO))
		{
			// ���ID��NAME
			WritePrivateProfileStringA("module", "id", "", theApp.m_strConf.c_str());
			WritePrivateProfileStringA("module", "name", "", theApp.m_strConf.c_str());
			theApp.SetReStart();
		}
		pThis->m_bExit = TRUE;
		pThis->m_bKeeeperExit = TRUE;
		OutputDebugStringA("keepProc �߳����˳���\n");
		pThis->SendMessage(WM_CLOSE, 0, 0);
		return 0xdead001;
	}
	char arg[500] = {};
	// ������Ҫ���������ĳ��򣬴˲�����Ҫ����
	GetPrivateProfileStringA("module", "arg", "", arg, sizeof(arg), theApp.m_strConf.c_str());
	// �������Ƶ������������һ���������������ػ�
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
		// ����ѭ���������ػ�����
		do{
			if (pThis->m_bIsStoped)
			{
				Sleep(20);
				continue;
			}

			// �ػ���������Ǳ�������������Ҫ�ٴ��������ػ�����
			if ((0 == pThis->m_handle || false==theApp.m_bUnique) && 0==theApp.m_nParentId )
			{
				CString lpParameters;
				// ������Ҫ���������ĳ��򣬴˲������ܲ���Ӱ��
				// �������Ϊdefault, ������������ָ���ػ�����ID
				lpParameters.Format(_T("-k %d"), int(GetCurrentProcessId()));
				if (strcmp(arg, "default")!=0)
				{
					lpParameters = CString(arg);
				} 
				SHELLEXECUTEINFO ShExecInfo = { 0 };
				ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
				ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
				ShExecInfo.hwnd = NULL;
				ShExecInfo.lpVerb = NULL;
				ShExecInfo.lpFile = Module;
				ShExecInfo.lpParameters = lpParameters;// �ò�������ָ�����ػ�����˭�ػ�
				ShExecInfo.lpDirectory = NULL;
				ShExecInfo.nShow = pThis->IsVisible() ? SW_SHOW : SW_HIDE;
				ShExecInfo.hInstApp = NULL;
				if (TRUE == ShellExecuteEx(&ShExecInfo))
				{
					if( pThis->m_nAffinityCpu && // ����ֵ����Ϊ0��������ĳ��CPU�ں�
						!SetProcessAffinityMask(ShExecInfo.hProcess, 1<<(pThis->m_nAffinityCpu - 1)) )
						OutputDebugString(Module + _T("\r\n��CPUʧ��!"));
					pThis->m_handle = ShExecInfo.hProcess;
					if(CheckWowProcess(ShExecInfo.hProcess) && 64==GetSystemBits())
					{
						pThis->Notice("��ʹ��64λ�ػ�����, ������ܵ������벻���Ľ��!");
					}
				}
			}
			
			// ���������ж�����ػ��������ڵ�2,3,...���ػ������������ʱ���ػ������˳�
			if( (bFind && false == theApp.m_bUnique) && 0 == theApp.m_nParentId )
				break;

			pThis->Lock();
			pThis->mytick.set_beginTime(GetStartTime(pThis->m_handle));
			pThis->m_ThreadId = ::GetProcessId(pThis->m_handle);
			pThis->m_cpu.setpid(pThis->m_ThreadId);
			pThis->m_nRunTimes ++;
			pThis->Refresh();
			pThis->Unlock();

			// ����̨�������⴦��
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

			// �ȴ������˳�
			const HANDLE handles[] = {pThis->m_handle, pThis->m_event};
			WaitForMultipleObjects(2, handles, FALSE, INFINITE);
			bFind = FALSE;
			theApp.m_nParentId = 0;// ���ػ������˳����ػ���������������

			// ���2������֮��С��TIME_SEC����Ƶ������������һ�������ر��ػ�
			time_t TIME = pThis->mytick.time();
			count = (TIME < TIME_SEC) ? count + 1 : 0;
			if (count == K_max)
			{
				pThis->EnableWindow(FALSE);
				pThis->ShowWindow(SW_HIDE);
				CString strPrompt = Module + _T("\r\nƵ������������! �Ƿ��˳��ػ�?");
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
			sprintf_s(szLog, "%s [%s:%u]��������ʱ��: %s.%s%s\n", szLog, pThis->m_moduleName, 
				pThis->m_ThreadId, pThis->log_runtime(true), pThis->m_bExit ? "[�ػ��˳�]" : "", 
				pThis->m_bIsStoped ? "[Զ��ֹͣ]" : "");
#if USING_GLOG
			logInfo << szLog;
#else 
			OutputDebugStringA(szLog);
			char filename[_MAX_PATH];
			sprintf_s(filename, "%s\\%d��%d���ػ���־.txt", pThis->m_sLogDir, 
				1900 + date->tm_year, 1 + date->tm_mon);
			FILE *m_fp = fopen(filename, "a+");
			if (m_fp)
			{
				fwrite(szLog, strlen(szLog), 1, m_fp);
				fclose(m_fp);
			}
#endif

			DWORD code = 0;// �˳�����
			GetExitCodeProcess(pThis->m_handle, &code);
			CloseHandle(pThis->m_handle);
			// ���������Ϊֹͣ״̬�������˳��ػ�����
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
	OutputDebugStringA("keepProc �߳����˳���\n");
	return 0xdead001;
}


// ������
DWORD WINAPI CKeeperDlg::checkProc(LPVOID pParam)
{
	CKeeperDlg *pThis = (CKeeperDlg*)pParam;
	pThis->m_bCheckExit = FALSE;
	OutputDebugStringA("checkProc �߳���������\n");
	const char *strFileName = pThis->m_FileDescription;// �ػ����������
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
	OutputDebugStringA("checkProc �߳����˳���\n");
	return 0xdead002;
}


// Զ��ͨ��
DWORD WINAPI CKeeperDlg::socketProc(LPVOID pParam)
{
	CKeeperDlg *pThis = (CKeeperDlg*)pParam;
	pThis->m_bSocketExit = FALSE;
	OutputDebugStringA("socketProc �߳���������\n");
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
			// ����Ƶ������Զ��IP
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
			bool flag = true; // �Ƿ����־
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
		// ����
		time_t cur = time(0);
		if (cur - tm >= pThis->m_nAliveTime)
		{
			tm = pSock->IsConnected() ? cur : 0;
			pThis->Refresh();
		}
		// ���������޻ظ�
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
	OutputDebugStringA("socketProc �߳����˳���\n");
	return 0xdead003;
}


void CKeeperDlg::ReStartSelf(const CString &Path)
{
	theApp.SetReStart();
}


// �˳��ػ�����(bExitDst - �Ƿ��˳����ػ�����)
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
		if (IDOK == MessageBox(_T("�����˳��ػ�������Ҫ�˳����ػ�������?"), 
			_T("����"), MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2))
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
		if (IDOK == MessageBox(_T("�����˳��ػ�������Ҫ�˳����ػ�������?"), 
			_T("����"), MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON2))
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
		strcpy_s(m_sTitle, m_modulePath);// ������ģ��·���ָ����ڱ���
		InitTitles(m_sTitle);
		if (hWnd = FindKeepWindow())
		{
			::ShowWindow(hWnd, ::IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
			DeleteCloseMenu(hWnd);
			::SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
			::SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)m_hIcon);
			WritePrivateProfileStringA("settings", "title", m_sTitle, m_strConf.c_str());
		}else {
			Afx_MessageBox box(_T("���ݳ���Ĵ��ڱ���δʶ�𵽴��ھ������Ӧ��ͨ��\"����->�Ҽ�->����\"Ϊ�����ñ��⡣"));
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
	char pRegName[64] = { 0 };// ע�����Ŀ����
	sprintf(pRegName, "Keep_%s", m_moduleId);
	m_bAutoRun = !m_bAutoRun;
	if(FALSE == SetSelfStart(m_pKeeperPath, pRegName, m_bAutoRun))
	{
		MessageBox(CString(m_pKeeperPath) + _T("\r\n����ʧ��!"), _T("����"), MB_ICONERROR);
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


// ����ʱ���ضԻ���
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


// �����ػ�����Ĳ���
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
		dlg.m_strRemoteIp = (L"δ����" == dlg.m_strRemoteIp ? L"" : dlg.m_strRemoteIp);
		// Զ����Ϣ�޸ĺ���Ҫ���·�������
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
					Afx_MessageBox dlg(_T("����ֹͣ����Զ���豸! �����ֶ��༭�����ļ�, ����ù��ܲ��ٿ���!"), 8000);
					dlg.DoModal();
					if (m_finder)
						m_finder->Exit();
				}
			}
		}
		USES_CONVERSION;
		const char *s_ip = W2A(ip);
		if (title != dlg.m_strTitle) // ����
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
		if (m_strIcon != dlg.m_strIcon) // ͼ��
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

// ���ڼ�¼���ػ������״̬
void CKeeperDlg::OnTimer(UINT_PTR nIDEvent)
{
	if (1 == nIDEvent && m_handle)
	{
		time_t TM = time(NULL);
		char content[256];
		int n = GetProcessId(m_handle);
		sprintf_s(content, "���� %d, �ڴ� %.2fM, �߳� %d, ��� %d.", n,
			m_cpu.get_mem_usage(), 
			GetThreadCount(n),
			m_cpu.get_handles_count());
		log_command(&TM, content);
	}
	CDialog::OnTimer(nIDEvent);
}


int CKeeperDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	const char keys[] = "0123456789";
	for (const char *p = keys; *p; ++p)
	{
		if(RegisterHotKey(m_hWnd, MY_HOTKEY, MOD_CONTROL, *p))
		{
			TRACE("======> ���ÿ�ݼ�\"Ctrl+%c\"�ɹ�\n", *p);
			RegisterHotKey(m_hWnd, MY_HOTKEY2, MOD_SHIFT, *p);
			break;
		}
	}

	return 0;
}


void CKeeperDlg::OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2)
{
	if (nHotKeyId == MY_HOTKEY)
	{
		OnSettings();// �������öԻ���
	}else if(nHotKeyId == MY_HOTKEY2){
		char buf[64+_MAX_PATH];
		sprintf_s(buf, "Keeper[%d]�����ػ�����:\r\n%s", GetCurrentProcessId(), m_modulePath);
		CString tip = CString(buf);
		Afx_MessageBox box(tip);
		box.DoModal();
	}

	CDialog::OnHotKey(nHotKeyId, nKey1, nKey2);
}
