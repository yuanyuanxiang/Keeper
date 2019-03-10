// Afx_MessageBox.cpp : 实现文件
//

#include "stdafx.h"
#include "Keeper.h"
#include "Afx_MessageBox.h"
#include "afxdialogex.h"

#define AUTO_KILL 1

// Afx_MessageBox 对话框

IMPLEMENT_DYNAMIC(Afx_MessageBox, CDialog)

Afx_MessageBox::Afx_MessageBox(LPCTSTR lpszPrompt, int nElapse, CWnd* pParent)
	: CDialog(Afx_MessageBox::IDD, pParent)
{
	m_nElapse = nElapse;
	m_strPrompt = lpszPrompt;
	m_nTopMost = 0;
	m_bIsNotice = false;
}

Afx_MessageBox::~Afx_MessageBox()
{
}

void Afx_MessageBox::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_INFOMATION, m_Infomation);
}


BEGIN_MESSAGE_MAP(Afx_MessageBox, CDialog)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// Afx_MessageBox 消息处理程序


BOOL Afx_MessageBox::OnInitDialog()
{
	CDialog::OnInitDialog();

	if (m_Infomation.GetSafeHwnd())
		m_Infomation.SetWindowText(m_strPrompt);

	SetTimer(AUTO_KILL, max(m_nElapse, 3333), NULL);

	if (m_nTopMost)
	{
		// https://blog.csdn.net/hately6104/article/details/70850573
		HWND hForegdWnd = ::GetForegroundWindow();
		DWORD dwCurID = ::GetCurrentThreadId();
		DWORD dwForeID = ::GetWindowThreadProcessId(hForegdWnd, NULL);
		::AttachThreadInput(dwCurID, dwForeID, TRUE);
		::SetForegroundWindow(GetSafeHwnd());
		::AttachThreadInput(dwCurID, dwForeID, FALSE);
	}
	if (m_bIsNotice)
	{
		GetDlgItem(IDOK)->ShowWindow(SW_HIDE);
		GetDlgItem(IDCANCEL)->ShowWindow(SW_HIDE);
	}
	if (!m_strTitle.IsEmpty())
		SetWindowText(m_strTitle);

	return TRUE;
}


void Afx_MessageBox::OnTimer(UINT_PTR nIDEvent)
{
	if (AUTO_KILL == nIDEvent)
	{
		KillTimer(AUTO_KILL);
		SendMessage(WM_CLOSE, 0, 0);
		return;
	}

	CDialog::OnTimer(nIDEvent);
}
