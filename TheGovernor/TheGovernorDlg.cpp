// The Governor
// Oct 16, 2005 - Greg Hoglund
// www.rootkit.com

#include "stdafx.h"
#include "TheGovernor.h"
#include "TheGovernorDlg.h"
#include ".\thegovernordlg.h"
#include "InjectDLL.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CListCtrl *g_list_ctrl = NULL;

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
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


// CTheGovernorDlg dialog



CTheGovernorDlg::CTheGovernorDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CTheGovernorDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_is_monitoring = FALSE;
}

void CTheGovernorDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST1, m_list_control);
}

BEGIN_MESSAGE_MAP(CTheGovernorDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(ID_MONITOR, OnBnClickedMonitor)
	ON_BN_CLICKED(ID_ABOUT, OnBnClickedAbout)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST1, OnLvnItemchangedList1)
END_MESSAGE_MAP()


// CTheGovernorDlg message handlers

BOOL CTheGovernorDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	EnableDebugPriv();

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	RECT rect;
	m_list_control.GetClientRect(&rect);
	m_list_control.InsertColumn(0, "Report");
	m_list_control.SetColumnWidth(0, rect.right);
	g_list_ctrl = &m_list_control;

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CTheGovernorDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTheGovernorDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTheGovernorDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTheGovernorDlg::OnBnClickedMonitor()
{
	// TODO: Add your control notification handler code here
	if(FALSE == m_is_monitoring)
	{
		if(TRUE == AttachToWOW())
		{
			m_is_monitoring = TRUE;
			GetDlgItem(ID_MONITOR)->EnableWindow(FALSE);
			logprintf("Attached to WOW.EXE and now monitoring");
		}
		else
		{
			MessageBox("Could not attach to World of Warcraft");
		}
	}
}

void CTheGovernorDlg::OnBnClickedAbout()
{
	// TODO: Add your control notification handler code here
}

void CTheGovernorDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	OnCancel();
}

BOOL CTheGovernorDlg::AttachToWOW()
{
	DWORD pid = GetPIDForProcess("WOW.EXE");
	if(pid)
	{
		DWORD ret = InjectDLLByPID(pid);
		if(ret) return TRUE;
	}

	return FALSE;
}

// logging function
// ---------------------------------------------------------------------------------
FILE *gLogFile = NULL;

void __cdecl logprintf (
        const char *format,
        ...
        )
{
        va_list arglist;
        va_start(arglist, format);

		char _t[1024];
		_vsnprintf(_t, 1022, format, arglist);
		
		OutputDebugString(_t);

		if(g_list_ctrl)
		{
			g_list_ctrl->InsertItem(0, _t);
		}

// uncomment this section if you want to use a logfile
#if 0
		if(NULL == gLogFile)
		{
			// changed this to destroy old file first
			gLogFile = fopen("C:\\GOVERNOR.LOG", "w");
		}


		if(gLogFile)
		{
			SYSTEMTIME stime;
			GetLocalTime(&stime);
			fprintf(gLogFile, "[ %d ][ %d %d %d %d ] %s \r\n",	GetCurrentThreadId(),
																stime.wHour,
																stime.wMinute,
																stime.wSecond,
																stime.wMilliseconds,
																_t);
		}
#endif

}



void CTheGovernorDlg::OnLvnItemchangedList1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here
	*pResult = 0;
}
