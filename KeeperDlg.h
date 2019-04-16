
// KeeperDlg.h : 头文件
//

#pragma once
#include <string>
#include "BcecrSocket.h"
#include "AppInfo.h"
#include "CPUusage.h"
#include "tick.h"
#include "ThreadPool\ServerFinder.h"
#include "Resource.h"
using namespace std;

class CKeeperDlg;
extern CKeeperDlg *g_KeeperDlg;

// Ctrl+数字 调出设置对话框
#define MY_HOTKEY 1

// 在条件c成立时等待s秒
#define WAIT(c, s) for(int n = max(100*(s), 2); --n && (c); Sleep(10))

/// 托盘消息
#define WM_TRAY_MSG (WM_USER+100)

#define MAX_NAMES 5

// 一次接收数据量
#define SOCKET_BUF 256

// Win7版本号
#define WINDOWS_7_VER 61

// stop application
#define StopApp ReStart

enum { S_RUN = 0, S_STOP, S_PAUSE };

// 64位程序需使用64位守护程序（如果不匹配返回true）
inline bool CheckWowProcess(HANDLE process)
{
	BOOL b32 = FALSE, b = IsWow64Process(process, &b32);
	return b32 ? false : sizeof(INT_PTR) != 8;
}

// Notice线程计数器
struct NoticeNum
{
	int num;
	CRITICAL_SECTION cs;
	NoticeNum() : num(0) { ::InitializeCriticalSection(&cs); }
	~NoticeNum() { while(num) Sleep(10); ::DeleteCriticalSection(&cs); }
	void AddNoticeNum(int n) { EnterCriticalSection(&cs); num += n; LeaveCriticalSection(&cs); }
};

// CKeeperDlg 对话框
class CKeeperDlg : public CDialog
{
// 构造
public:
	CKeeperDlg(const string& Icon = "", CWnd* pParent = NULL);	// 标准构造函数

	~CKeeperDlg();

	// 获取APP名称
	const char* GetAppName() const { return m_moduleName; }
	// 获取APP唯一ID
	const char* GetAppId() const { return m_moduleId; }
	// 获取APP的密码
	const char* GetAppPwd() const { return m_password; }
	// 获取远程升级服务器的IP
	const char* GetRemoteIp() const { return m_strUpServer[0] ? m_strUpServer : m_pSocket->m_chToIp; }
	// 获取Keeper路径
	const char* GetModulePath() const { return m_pKeeperPath; }

	friend void UpdateThread(void *param);

	friend void RecoverThread(void *param);

private:
	enum { IDD = IDD_Keeper_DIALOG };// 对话框数据

	//////////////////////////////////////////////////////////////////////////
	char m_moduleName[32];			// 模块名称小写【必须配置】
	char m_moduleId[32];			// 模块编码【必须配置】
	char m_password[64];			// 密码
	char m_modulePath[MAX_PATH];	// 模块路径小写
	char m_pKeeperPath[MAX_PATH];	// 本程序路径
	int m_bIsStoped;				// 被守护程序状态:0-运行,1-停止,2-暂停
	bool m_bUpdate;					// 是否正在升级
	char m_strUpServer[64];			// 升级服务器
	string m_strConf;				// 配置文件路径
	char m_FileDescription[MAX_PATH];// 被守护程序描述

	//////////////////////////////////////////////////////////////////////////
	// settings
	int m_nWatchTime;				// 观测时间[2]
	bool m_bAutoRun;				// 开启自启[3]
	int m_nVisible;					// 被守护程序是否可见[4]
	int m_nAffinityCpu;				// 绑定的CPU[5]
	char m_sTitle[MAX_PATH];		// 窗口名称(MFC程序需配置)[6]
	CString m_strIcon;				// 程序图标[7]
	char m_Ip[64];					// 远端Ip[8]
	int m_nPort;					// 远端端口[9]
	int m_nExitCode;				// 被守护程序正常退出代码[10]

	CBcecrSocket *m_pSocket;		// 通讯socket
	CString m_sWindowNames[MAX_NAMES];// 被守护的程序可能显示的窗口名
	char m_sLogDir[MAX_PATH];		// 日志目录

	BOOL m_bExit;					// 主程序退出
	BOOL m_bKeeeperExit;			// 守护线程退出
	BOOL m_bCheckExit;				// 保活线程退出
	BOOL m_bSocketExit;				// Socket线程退出
	DWORD m_ThreadId;				// 被守护进程的ID
	HANDLE m_handle;				// 被守护进程的句柄
	int m_nAliveTime;				// 心跳周期（秒）
	int m_nRunTimes;				// 被守护程序启动次数
	char m_sRunLog[64];				// 被守护程序运行日志
	HANDLE m_event;					// keeper退出事件

	BOOL m_bTray;					// 托盘标记
	HICON m_hIcon;					// 图标句柄
	Gdiplus::Bitmap *pBmp;			// 图标
	CMenu m_trayPopupMenu;			// 托盘右键菜单
	BOOL SetTray(BOOL bTray);		// 设置托盘效果

	CPUusage m_cpu;					// CPU使用率
	tick_s	mytick;					// tick
	char m_strCreateTime[64];		// 创建日期
	char m_strModeTime[64];			// 修改日期
	char m_strVersion[64];			// 文件版本
	float m_fFileSize;				// 文件大小(Mb)
	char m_strKeeperVer[64];		// 守护程序版本
	CRITICAL_SECTION m_cs;

	void GetFileTimeInfo(const char *file); // 获取文件信息

	void GetFileInfo(); // 获取被守护程序文件信息

	void GetDiskInfo(char *info); // 获取程序所在磁盘的信息

	// 下载文件
	bool DownloadFile(const char *name, const char *postfix = ".exe", BOOL isKeeper = true, const char *type = "exe");

	void DownloadFilelist(const char *dst, char *p);

	// 从内存释放升级器"updater.exe"
	BOOL ReleaseRes(const char *strFileName, WORD wResID, const CString &strFileType);

	CServerFinder *m_finder;

	const char *ControlIp() const { return (m_finder && m_finder->IsReady()) ? CMyTask::GetIp() : m_Ip; }

	void Lock() { EnterCriticalSection(&m_cs); }

	void Unlock() { LeaveCriticalSection(&m_cs); }

public:

	//////////////////////////////////////////////////////////////////////////
	// 守护线程
	static DWORD WINAPI keepProc(LPVOID pParam);

	// 检测假死
	static DWORD WINAPI checkProc(LPVOID pParam);

	// 执行远端命令
	static DWORD WINAPI socketProc(LPVOID pParam);
	//////////////////////////////////////////////////////////////////////////

	// 获取本机IP
	std::string GetLocalHost() const;

	// 设置本身开机自启动
	BOOL SetSelfStart(const char *sPath, const char *sNmae, bool bEnable = true) const;

	// 找到被守护程序的句柄
	HWND FindKeepWindow() const;

	// 获取被守护程序的进程句柄
	static HANDLE GetProcessHandle(const CString &processName, const CString &strFullPath, BOOL &op);

	// 是否启动时可见
	bool IsVisible() const { return 1 == m_nVisible; }

	// 为被守护程序加载图标
	HICON GetIcon() const { return m_hIcon; }

	// 获取守护程序路径
	CString GetKeeperPath() const { return CString(m_pKeeperPath); }

	// 退出守护程序
	void ExitKeeper(bool bExitDst = false);

	// 重启守护程序
	static void ReStartSelf(const CString &Path);

	// 启动ffmepg传送屏幕
	void ffmepgStart(int nPort);

	// 停止ffmpeg传送屏幕
	void ffmpegStop(const char *app);

	// 向监控端发送消息
	void SendInfo(const char *info, const char *details, int code = 0);

// 实现
protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

	afx_msg LRESULT OnTrayCallbackMsg(WPARAM wp, LPARAM lp);// 托盘处理函数

	// 隐藏到托盘
	BOOL HideToTray();

	// 停止程序并关机
	void Shutdown();

	// 重启计算机
	void ReBoot();

	// 重启程序
	bool ReStart(bool bForce = true);

	// 刷新程序
	void Refresh();

	// 停止程序
	void Stop(bool bAll=false);

	// 启动程序
	void Start();

	// 升级程序
	void Update(const std::string &arg);

	// 设置时间
	void SetTime(const std::string &arg);

	// 查询进程信息
	bool QueryAppInfo(AppInfo &info);

	// 计算程序运行时间并记录
	const char* log_runtime(bool bLog2File = false);

	// 记录远端监控操作日志
	void log_command(const time_t *date, const char *ip, const char *cmd, const char *arg) const;

	void log_command(const time_t *date, const char *content) const;

	void InitRemoteIp(const char *ip, int port);

	// 初始化名称数组(程序根据该数组寻找窗口句柄)
	void InitTitles(const char *t);

	// 设置对话框图标
	void SetDlgIcon(const CString &icon);

	// 发布公告
	void Notice(const char *notice, int tm=8000);

	// 启动传送线程，以供监控端监视程序状态
	void Watch(int bWatch);

	// 退出守护
	afx_msg void OnExitMenu();

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	afx_msg void OnDestroy();
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnBnClickedButtonShowconsole();
	afx_msg void OnAppAbout();
	afx_msg void OnSelfStart();
	afx_msg void OnUpdateSelfStart(CCmdUI *pCmdUI);
	afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);
public:
	afx_msg void OnSettings();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2);
};
