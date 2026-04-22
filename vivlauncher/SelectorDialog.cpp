#include "SelectorDialog.h"
#include "vivlauncher.h"
#include <string>
#include <vector>

extern HINSTANCE hInst;
static std::vector<VivadoInstall> g_installs;
static std::wstring g_xprFilePath;
static int g_selectedIndex = 0;
static WNDPROC g_oldListBoxProc = nullptr;

static void ShowHelp(HWND hWnd)
{
    MessageBoxW(hWnd,
        L"Keyboard Shortcuts:\n\n"
        L"  j, Down : Next version\n"
        L"  k, Up   : Previous version\n"
        L"  Enter, Space : Launch selected version\n"
        L"  ?, h    : Show this help\n"
        L"  q, Esc  : Cancel and Exit\n",
        L"Vivado Launcher Help",
        MB_OK | MB_ICONINFORMATION);
}
static LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_CHAR:
    {
        wchar_t ch = (wchar_t)wParam;
        switch (ch)
        {
        case L'j': PostMessageW(hWnd, WM_KEYDOWN, VK_DOWN, 0); return 0;
        case L'k': PostMessageW(hWnd, WM_KEYDOWN, VK_UP, 0); return 0;
        case L'?': ShowHelp(GetParent(hWnd)); return 0;
        case L'h': ShowHelp(GetParent(hWnd)); return 0;
        case L'q': EndDialog(GetParent(hWnd), 0); return 0;
        }
        break;
    }
    case WM_KEYDOWN:
    {
        switch (wParam)
        {
        case VK_RETURN:
        case VK_SPACE:
        {
            int selIdx = (int)SendMessageW(hWnd, LB_GETCURSEL, 0, 0);
            if (selIdx >= 0)
            {
                int origIdx = (int)SendMessageW(hWnd, LB_GETITEMDATA, selIdx, 0);
                EndDialog(GetParent(hWnd), origIdx + 1);
            }
            return 0;
        }
        case VK_ESCAPE:
            EndDialog(GetParent(hWnd), 0);
            return 0;
        }
        break;
    }
    }
    return CallWindowProcW(g_oldListBoxProc, hWnd, message, wParam, lParam);
}

static void PopulateList(HWND hList)
{
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);

    for (size_t i = 0; i < g_installs.size(); ++i)
    {
        const auto& inst = g_installs[i];
        std::wstring display = inst.version + L"  (" + inst.path + L")";
        int idx = (int)SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        SendMessageW(hList, LB_SETITEMDATA, idx, (LPARAM)i);
    }
}

INT_PTR CALLBACK SelectorDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        POINT pt;
        GetCursorPos(&pt);

        RECT rc;
        GetWindowRect(hDlg, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;

        int x = pt.x - width / 2;
        int y = pt.y - height / 2;

        HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = {};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMonitor, &mi))
        {
            x = max(mi.rcWork.left, min(x, mi.rcWork.right - width));
            y = max(mi.rcWork.top, min(y, mi.rcWork.bottom - height));
        }

        SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        g_installs = *(std::vector<VivadoInstall>*)lParam;
        HWND hList = GetDlgItem(hDlg, IDC_VERSION_LIST);
        PopulateList(hList);

        if (!g_installs.empty())
        {
            SendMessageW(hList, LB_SETCURSEL, 0, 0);
            g_selectedIndex = 0;
        }

        g_oldListBoxProc = (WNDPROC)SetWindowLongPtrW(hList, GWLP_WNDPROC, (LONG_PTR)ListBoxSubclassProc);

        SetFocus(hList);
        return (INT_PTR)FALSE;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);

        if (id == IDC_VERSION_LIST && HIWORD(wParam) == LBN_DBLCLK)
        {
            HWND hList = GetDlgItem(hDlg, IDC_VERSION_LIST);
            int selIdx = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (selIdx >= 0)
            {
                int origIdx = (int)SendMessageW(hList, LB_GETITEMDATA, selIdx, 0);
                EndDialog(hDlg, origIdx + 1);
            }
            return TRUE;
        }

        if (id == IDCANCEL)
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }
        break;
    }
    }

    return (INT_PTR)FALSE;
}

INT_PTR CALLBACK AddPathDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            HWND hEdit = GetDlgItem(hDlg, IDC_PATH_EDIT);
            wchar_t path[MAX_PATH] = {};
            GetWindowTextW(hEdit, path, MAX_PATH);

            if (path[0])
            {
                g_installs = AddCustomPath(path);
                EndDialog(hDlg, 1);
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_BROWSE_BUTTON)
        {
            OPENFILENAMEW ofn = {};
            wchar_t path[MAX_PATH] = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = L"Select Vivado Installation Folder";

            if (GetOpenFileNameW(&ofn))
            {
                SetWindowTextW(GetDlgItem(hDlg, IDC_PATH_EDIT), path);
            }
            return TRUE;
        }
        break;
    }
    }

    return (INT_PTR)FALSE;
}
