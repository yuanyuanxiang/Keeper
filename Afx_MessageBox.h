#pragma once
#include "afxwin.h"


// Afx_MessageBox 对话框
// 该对话框会自动关闭
class Afx_MessageBox : public CDialog
{
	DECLARE_DYNAMIC(Afx_MessageBox)

public:
	Afx_MessageBox(LPCTSTR lpszPrompt, int nElapse = 5000, CWnd* pParent = NULL);   // 标准构造函数
	virtual ~Afx_MessageBox();

	// 对话框数据
	enum { IDD = IDD_AFXMESSAGE_BOX };

	int m_nElapse;

	int m_nTopMost;

	bool m_bIsNotice; // 是否为提示信息

	CString m_strPrompt;

	CString m_strTitle; // 窗口标题（可选）

	void SetTopMost() { m_nTopMost = 1; }

	void SetNotice() { m_bIsNotice = true; }

	void SetTitle(const char *title) { m_strTitle = CString(title); }

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_Infomation;
	virtual BOOL OnInitDialog();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
