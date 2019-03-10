#pragma once
#include "afxwin.h"
#include <vector>
using namespace std;

// CChooseExecutable 对话框

/**
* @class CChooseExecutable
* @brief 选择对话框，选择被守护程序
* 2018-2-22
*/
class CChooseExecutable : public CDialog
{
	DECLARE_DYNAMIC(CChooseExecutable)

public:
	CChooseExecutable(const vector<CString> &files, CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CChooseExecutable();

	vector<CString> m_Files;

	CString GetExecutable() const;
	
// 对话框数据
	enum { IDD = IDD_EXECUTABLE_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CComboBox m_ComboExecutable;
	CString m_StrExecutable;
	virtual BOOL OnInitDialog();
};
