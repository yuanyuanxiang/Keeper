// KeeperSettings.cpp : 实现文件
//

#include "stdafx.h"
#include "Keeper.h"
#include "KeeperSettings.h"
#include "afxdialogex.h"
#include "KeeperDlg.h"


// CKeeperSettings 对话框

IMPLEMENT_DYNAMIC(CKeeperSettings, CDialog)

CKeeperSettings::CKeeperSettings(const string &conf, CWnd* pParent)
	: CDialog(CKeeperSettings::IDD, pParent)
	, m_sConf(conf)
	, m_nWatchTime(0)
	, m_nCpu(0)
	, m_nVisible(0)
	, m_strRemoteIp(_T(""))
	, m_nRemotePort(0)
	, m_strTitle(_T(""))
	, m_strIcon(_T(""))
{
}

CKeeperSettings::~CKeeperSettings()
{
}

void CKeeperSettings::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_WATCH_TIME, m_EditWatchTime);
	DDX_Text(pDX, IDC_EDIT_WATCH_TIME, m_nWatchTime);
	DDX_Control(pDX, IDC_COMBO_CPU, m_ComboCpu);
	DDX_CBIndex(pDX, IDC_COMBO_CPU, m_nCpu);
	DDX_Control(pDX, IDC_EDIT_REMOTE, m_EditRemoteIp);
	DDX_Text(pDX, IDC_EDIT_REMOTE, m_strRemoteIp);
	DDX_Control(pDX, IDC_EDIT_PORT, m_EditRemotePort);
	DDX_Text(pDX, IDC_EDIT_PORT, m_nRemotePort);
	DDX_Control(pDX, IDC_EDIT_TITLE, m_EditTitle);
	DDX_Text(pDX, IDC_EDIT_TITLE, m_strTitle);
	DDX_Control(pDX, IDC_EDIT_ICON, m_EditIcon);
	DDX_Text(pDX, IDC_EDIT_ICON, m_strIcon);
	DDV_MinMaxInt(pDX, m_nWatchTime, 1, 120000);
	DDV_MinMaxInt(pDX, m_nRemotePort, 1024, 65535);
	DDX_Control(pDX, IDC_RADIO_VISIBLE, m_ButtonVisible);
}


BEGIN_MESSAGE_MAP(CKeeperSettings, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_ICON, &CKeeperSettings::OnBnClickedButtonIcon)
	ON_BN_CLICKED(IDC_BUTTON_RESET, &CKeeperSettings::OnBnClickedButtonReset)
	ON_BN_CLICKED(IDC_RADIO_VISIBLE, &CKeeperSettings::OnBnClickedRadioVisible)
END_MESSAGE_MAP()


// CKeeperSettings 消息处理程序


void CKeeperSettings::OnBnClickedButtonIcon()
{
	CFileDialog dlgFile(TRUE, NULL, NULL, OFN_HIDEREADONLY, _T("Images|*.jpg;*.png;*.bmp;*.ico|All Files(*.*)|*.*||"), NULL);
	if (IDOK == dlgFile.DoModal())
	{
		m_strIcon = dlgFile.GetPathName();
		m_EditIcon.SetWindowText(m_strIcon);
	}
}


void CKeeperSettings::OnBnClickedButtonReset()
{
	if (IDYES == MessageBox(_T("重置配置将退出守护程序,下次重启生效!是否继续?"), _T("提示"), MB_ICONINFORMATION | MB_YESNO))
	{
		DeleteFileA(m_sConf.c_str());
		SendMessage(WM_CLOSE);
		CKeeperDlg *keeper = (CKeeperDlg *)GetParent();
		CString path = keeper->GetKeeperPath();
		CKeeperDlg::ReStartSelf(path);
		keeper->ExitKeeper();
	}
}


void CKeeperSettings::OnBnClickedRadioVisible()
{
	m_nVisible = !m_nVisible;
	m_ButtonVisible.SetCheck(m_nVisible);
}


BOOL CKeeperSettings::OnInitDialog()
{
	CDialog::OnInitDialog();

	SYSTEM_INFO info;
	GetSystemInfo(&info);
	m_ComboCpu.InsertString(0, L"不绑定");
	for (int i = 0; i != info.dwNumberOfProcessors; ++i)
	{
		char buf[64];
		sprintf_s(buf, "核心%d", i + 1);
		m_ComboCpu.InsertString(i + 1, CString(buf));
	}
	m_ComboCpu.SetCurSel(m_nCpu > info.dwNumberOfProcessors ? 0 : m_nCpu);
	m_ButtonVisible.SetCheck(m_nVisible);

	bool bNotSet = (L"" == m_strRemoteIp);
	m_EditRemoteIp.SetWindowText(bNotSet ? L"未配置" : m_strRemoteIp);
	
	return TRUE;
}
