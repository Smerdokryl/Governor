// TheGovernorDlg.h : header file
//

#pragma once
#include "afxcmn.h"


// CTheGovernorDlg dialog
class CTheGovernorDlg : public CDialog
{
// Construction
public:
	CTheGovernorDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_THEGOVERNOR_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	BOOL AttachToWOW();

// Implementation
protected:
	HICON m_hIcon;

	BOOL m_is_monitoring;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedMonitor();
	afx_msg void OnBnClickedAbout();
	afx_msg void OnBnClickedOk();
	afx_msg void OnLvnItemchangedList1(NMHDR *pNMHDR, LRESULT *pResult);
	CListCtrl m_list_control;
};
