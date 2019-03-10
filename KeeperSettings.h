#pragma once
#include "afxwin.h"


// CKeeperSettings 对话框

class CKeeperSettings : public CDialog
{
	DECLARE_DYNAMIC(CKeeperSettings)

public:
	CKeeperSettings(const string &conf, CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CKeeperSettings();

	string m_sConf; // 配置文件

// 对话框数据
	enum { IDD = IDD_SETTINGS_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CEdit m_EditWatchTime;
	int m_nWatchTime;
	CComboBox m_ComboCpu;
	int m_nCpu;
	int m_nVisible;
	CEdit m_EditRemoteIp;
	CString m_strRemoteIp;
	CEdit m_EditRemotePort;
	int m_nRemotePort;
	CEdit m_EditTitle;
	CString m_strTitle;
	CEdit m_EditIcon;
	CString m_strIcon;
	afx_msg void OnBnClickedButtonIcon();
	afx_msg void OnBnClickedButtonReset();
	virtual BOOL OnInitDialog();
	CButton m_ButtonVisible;
	afx_msg void OnBnClickedRadioVisible();
};
