#include "SelectorDialog.h"
#include "vivlauncher.h"
#include <string>
#include <vector>
#include <filesystem>
#include <commctrl.h>

namespace fs = std::filesystem;

extern HINSTANCE hInst;
static std::vector<VivadoInstall> g_installs;
static std::wstring g_xprFilePath;
static int g_selectedIndex = 0;
static WNDPROC g_oldListBoxProc = nullptr;

static void ShowHelp(HWND hWnd)
{
    MessageBoxW(hWnd,
        L"Keyboard & Mouse Shortcuts:\n\n"
        L"  j, Down, Wheel Down : Next version\n"
        L"  k, Up, Wheel Up     : Previous version\n"
        L"  Enter, Space, L-DblClick : Launch selected\n"
        L"  Middle Click, q, Esc : Cancel and Exit\n"
        L"  ?, h : Show this help\n",
        L"Vivado Launcher Help",
        MB_OK | MB_ICONINFORMATION);
}
static LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_MOUSEWHEEL:
    {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta > 0) PostMessageW(hWnd, WM_KEYDOWN, VK_UP, 0);
        else if (delta < 0) PostMessageW(hWnd, WM_KEYDOWN, VK_DOWN, 0);
        return 0;
    }

    case WM_MBUTTONDOWN:
    {
        EndDialog(GetParent(hWnd), 0);
        return 0;
    }

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

static std::vector<VivadoInstall> g_mainInstalls;
static std::vector<RecentProject> g_mainRecent;
static LaunchSettings g_launchSettings;

static void SetMainTab(HWND hDlg, int tab)
{
    const int openControls[] = {
        IDC_PROJECT_LABEL, IDC_PROJECT_PATH, IDC_BROWSE_PROJECT,
        IDC_RECENT_LABEL, IDC_RECENT_PROJECTS, IDC_INSTALL_LABEL,
        IDC_INSTALL_LIST, IDC_BIND_PROJECT, IDC_OPEN_PROJECT
    };
    const int vivadoControls[] = {
        IDC_INSTALL_LABEL, IDC_INSTALL_LIST, IDC_REFRESH_INSTALLS,
        IDC_ADD_INSTALL, IDC_SET_DEFAULT
    };
    const int launchControls[] = {
        IDC_OPTIONS_LABEL, IDC_NO_LOG, IDC_NO_JOURNAL, IDC_EXTRA_ARGS
    };
    const int diagnosticControls[] = {
        IDC_REGISTER_XPR, IDC_DIAGNOSTIC_LABEL, IDC_VIEW_LOG,
        IDC_OPEN_LOG_FOLDER, IDC_CLEAR_RECENT, IDC_OPEN_CONFIG_FOLDER,
        IDC_LOG_PATH, IDC_CONFIG_PATH, IDC_LOG_LABEL, IDC_CONFIG_LABEL
    };

    for (int id : openControls) ShowWindow(GetDlgItem(hDlg, id), tab == 0 ? SW_SHOW : SW_HIDE);
    for (int id : vivadoControls) ShowWindow(GetDlgItem(hDlg, id), tab == 1 ? SW_SHOW : SW_HIDE);
    for (int id : launchControls) ShowWindow(GetDlgItem(hDlg, id), tab == 2 ? SW_SHOW : SW_HIDE);
    for (int id : diagnosticControls) ShowWindow(GetDlgItem(hDlg, id), tab == 3 ? SW_SHOW : SW_HIDE);
}

static void PopulateMainInstallList(HWND hDlg)
{
    HWND hList = GetDlgItem(hDlg, IDC_INSTALL_LIST);
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    std::wstring defaultVersion = LoadDefaultVersion();
    wchar_t projectPath[32768] = {};
    GetWindowTextW(GetDlgItem(hDlg, IDC_PROJECT_PATH), projectPath, ARRAYSIZE(projectPath));
    std::wstring boundVersion = LoadProjectVersion(projectPath);
    if (!boundVersion.empty())
        defaultVersion = boundVersion;
    int defaultIndex = -1;

    for (size_t i = 0; i < g_mainInstalls.size(); ++i)
    {
        const auto& install = g_mainInstalls[i];
        std::wstring text = install.version + L"    " + install.path;
        int item = (int)SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)text.c_str());
        SendMessageW(hList, LB_SETITEMDATA, item, (LPARAM)i);
        if (_wcsicmp(install.version.c_str(), defaultVersion.c_str()) == 0)
            defaultIndex = item;
    }

    if (defaultIndex >= 0)
        SendMessageW(hList, LB_SETCURSEL, defaultIndex, 0);
    else if (!g_mainInstalls.empty())
        SendMessageW(hList, LB_SETCURSEL, 0, 0);
}

static void PopulateRecentList(HWND hDlg)
{
    HWND hCombo = GetDlgItem(hDlg, IDC_RECENT_PROJECTS);
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    g_mainRecent = LoadRecentProjects();
    for (size_t i = 0; i < g_mainRecent.size(); ++i)
    {
        std::wstring display = fs::path(g_mainRecent[i].path).filename().native();
        display += L"  (" + g_mainRecent[i].path + L")";
        int item = (int)SendMessageW(hCombo, CB_ADDSTRING, 0, (LPARAM)display.c_str());
        SendMessageW(hCombo, CB_SETITEMDATA, item, (LPARAM)i);
    }
}

static void SetProjectPath(HWND hDlg, const std::wstring& path)
{
    SetWindowTextW(GetDlgItem(hDlg, IDC_PROJECT_PATH), path.c_str());
}

static void OpenShellPath(HWND hDlg, const std::wstring& path)
{
    if (path.empty())
        return;
    HINSTANCE result = ShellExecuteW(hDlg, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32)
    {
        MessageBoxW(hDlg, L"Windows could not open this path.", L"Vivado Launcher",
                    MB_OK | MB_ICONWARNING);
    }
}

static bool OpenMainProject(HWND hDlg)
{
    wchar_t path[32768] = {};
    GetWindowTextW(GetDlgItem(hDlg, IDC_PROJECT_PATH), path, ARRAYSIZE(path));
    std::wstring projectPath = GetAbsolutePath(path);
    if (projectPath.empty() || !fs::exists(projectPath))
    {
        MessageBoxW(hDlg, L"Please select an existing Vivado .xpr project.", L"Project not found",
                    MB_OK | MB_ICONWARNING);
        return false;
    }

    HWND hList = GetDlgItem(hDlg, IDC_INSTALL_LIST);
    int selected = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
    if (selected < 0)
    {
        MessageBoxW(hDlg, L"No valid Vivado installation is available.", L"Vivado Launcher",
                    MB_OK | MB_ICONERROR);
        return false;
    }

    int index = (int)SendMessageW(hList, LB_GETITEMDATA, selected, 0);
    if (index < 0 || index >= (int)g_mainInstalls.size())
        return false;

    const auto& install = g_mainInstalls[index];
    std::wstring boundVersion = LoadProjectVersion(projectPath);
    if (!boundVersion.empty() && _wcsicmp(boundVersion.c_str(), install.version.c_str()) != 0)
    {
        std::wstring message = L"This project is bound to Vivado " + boundVersion +
                               L", but Vivado " + install.version + L" is selected.\n\nContinue anyway?";
        if (MessageBoxW(hDlg, message.c_str(), L"Vivado version mismatch",
                        MB_YESNO | MB_ICONWARNING) != IDYES)
            return false;
    }
    RememberRecentProject(projectPath, install.version);
    g_launchSettings.noLog = IsDlgButtonChecked(hDlg, IDC_NO_LOG) == BST_CHECKED;
    g_launchSettings.noJournal = IsDlgButtonChecked(hDlg, IDC_NO_JOURNAL) == BST_CHECKED;
    wchar_t extraArgs[32768] = {};
    GetWindowTextW(GetDlgItem(hDlg, IDC_EXTRA_ARGS), extraArgs, ARRAYSIZE(extraArgs));
    g_launchSettings.extraArgs = extraArgs;
    SaveLaunchSettings(g_launchSettings);
    if (!LaunchVivado(install.exePath.c_str(), projectPath.c_str(), g_launchSettings))
    {
        MessageBoxW(hDlg, L"Vivado could not be started. Check the installation path.",
                    L"Launch failed", MB_OK | MB_ICONERROR);
        return false;
    }
    EndDialog(hDlg, IDOK);
    return true;
}

INT_PTR CALLBACK MainDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        HWND hTabs = GetDlgItem(hDlg, IDC_MAIN_TABS);
        TCITEMW tab = {};
        tab.mask = TCIF_TEXT;
        const wchar_t* tabNames[] = { L"Open project", L"Vivado", L"Launch", L"Diagnostics" };
        for (const wchar_t* name : tabNames)
        {
            tab.pszText = const_cast<wchar_t*>(name);
            TabCtrl_InsertItem(hTabs, TabCtrl_GetItemCount(hTabs), &tab);
        }
        TabCtrl_SetCurSel(hTabs, 0);
        const wchar_t* initialPath = (const wchar_t*)lParam;
        if (initialPath && *initialPath)
            SetProjectPath(hDlg, initialPath);
        g_mainInstalls = DetectVivadoInstallations();
        g_launchSettings = LoadLaunchSettings();
        CheckDlgButton(hDlg, IDC_NO_LOG, g_launchSettings.noLog ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_NO_JOURNAL, g_launchSettings.noJournal ? BST_CHECKED : BST_UNCHECKED);
        SetWindowTextW(GetDlgItem(hDlg, IDC_EXTRA_ARGS), g_launchSettings.extraArgs.c_str());
        SetWindowTextW(GetDlgItem(hDlg, IDC_LOG_PATH), GetLogFilePath().c_str());
        SetWindowTextW(GetDlgItem(hDlg, IDC_CONFIG_PATH), GetSettingsFilePath().c_str());
        PopulateMainInstallList(hDlg);
        PopulateRecentList(hDlg);
        SetMainTab(hDlg, 0);
        return TRUE;
    }
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->idFrom == IDC_MAIN_TABS &&
            ((LPNMHDR)lParam)->code == TCN_SELCHANGE)
        {
            SetMainTab(hDlg, TabCtrl_GetCurSel(GetDlgItem(hDlg, IDC_MAIN_TABS)));
            return TRUE;
        }
        break;
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == IDC_BROWSE_PROJECT)
        {
            OPENFILENAMEW ofn = {};
            wchar_t path[MAX_PATH] = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = L"Vivado Project (*.xpr)\0*.xpr\0All Files (*.*)\0*.*\0\0";
            ofn.lpstrFile = path;
            ofn.nMaxFile = ARRAYSIZE(path);
            if (GetOpenFileNameW(&ofn))
            {
                SetProjectPath(hDlg, path);
                PopulateMainInstallList(hDlg);
            }
            return TRUE;
        }
        if (id == IDC_RECENT_PROJECTS && HIWORD(wParam) == CBN_SELCHANGE)
        {
            int item = (int)SendMessageW((HWND)lParam, CB_GETCURSEL, 0, 0);
            if (item >= 0)
            {
                int index = (int)SendMessageW((HWND)lParam, CB_GETITEMDATA, item, 0);
                if (index >= 0 && index < (int)g_mainRecent.size())
                {
                    SetProjectPath(hDlg, g_mainRecent[index].path);
                    PopulateMainInstallList(hDlg);
                }
            }
            return TRUE;
        }
        if (id == IDC_REFRESH_INSTALLS)
        {
            g_mainInstalls = DetectVivadoInstallations();
            PopulateMainInstallList(hDlg);
            return TRUE;
        }
        if (id == IDC_SET_DEFAULT)
        {
            HWND hList = GetDlgItem(hDlg, IDC_INSTALL_LIST);
            int item = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (item >= 0)
            {
                int index = (int)SendMessageW(hList, LB_GETITEMDATA, item, 0);
                if (index >= 0 && index < (int)g_mainInstalls.size())
                {
                    SaveDefaultVersion(g_mainInstalls[index].version);
                    MessageBoxW(hDlg, L"The selected Vivado version is now the default.",
                                L"Vivado Launcher", MB_OK | MB_ICONINFORMATION);
                }
            }
            return TRUE;
        }
        if (id == IDC_BIND_PROJECT)
        {
            wchar_t path[32768] = {};
            GetWindowTextW(GetDlgItem(hDlg, IDC_PROJECT_PATH), path, ARRAYSIZE(path));
            std::wstring projectPath = GetAbsolutePath(path);
            HWND hList = GetDlgItem(hDlg, IDC_INSTALL_LIST);
            int item = (int)SendMessageW(hList, LB_GETCURSEL, 0, 0);
            if (projectPath.empty() || !fs::exists(projectPath) || item < 0)
            {
                MessageBoxW(hDlg, L"Select an existing project and Vivado version first.",
                            L"Vivado Launcher", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            int index = (int)SendMessageW(hList, LB_GETITEMDATA, item, 0);
            if (index >= 0 && index < (int)g_mainInstalls.size() &&
                SaveProjectVersion(projectPath, g_mainInstalls[index].version))
            {
                MessageBoxW(hDlg, L"This project will use the selected Vivado version next time.",
                            L"Vivado Launcher", MB_OK | MB_ICONINFORMATION);
            }
            return TRUE;
        }
        if (id == IDC_REGISTER_XPR)
        {
            bool registered = RegisterXprFileAssociation();
            MessageBoxW(hDlg, registered ? L".xpr files are now associated with vivlauncher."
                                         : L"Could not register the .xpr file association.",
                        L"Vivado Launcher", MB_OK | (registered ? MB_ICONINFORMATION : MB_ICONERROR));
            return TRUE;
        }
        if (id == IDC_VIEW_LOG)
        {
            auto logPath = GetLogFilePath();
            if (!fs::exists(logPath))
            {
                MessageBoxW(hDlg, L"No launch log has been created yet.", L"Vivado Launcher",
                            MB_OK | MB_ICONINFORMATION);
            }
            else
            {
                OpenShellPath(hDlg, logPath);
            }
            return TRUE;
        }
        if (id == IDC_OPEN_LOG_FOLDER || id == IDC_OPEN_CONFIG_FOLDER)
        {
            auto filePath = id == IDC_OPEN_LOG_FOLDER ? GetLogFilePath() : GetSettingsFilePath();
            OpenShellPath(hDlg, fs::path(filePath).parent_path().native());
            return TRUE;
        }
        if (id == IDC_CLEAR_RECENT)
        {
            if (MessageBoxW(hDlg, L"Clear the recent project list?", L"Vivado Launcher",
                            MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                if (ClearRecentProjects())
                {
                    PopulateRecentList(hDlg);
                    MessageBoxW(hDlg, L"Recent projects cleared.", L"Vivado Launcher",
                                MB_OK | MB_ICONINFORMATION);
                }
            }
            return TRUE;
        }
        if (id == IDC_ADD_INSTALL)
        {
            if (DialogBoxW(hInst, MAKEINTRESOURCE(IDD_ADD_PATH), hDlg, AddPathDialogProc) > 0)
            {
                g_mainInstalls = DetectVivadoInstallations();
                PopulateMainInstallList(hDlg);
            }
            return TRUE;
        }
        if (id == IDC_OPEN_PROJECT || (id == IDOK && HIWORD(wParam) == BN_CLICKED))
        {
            OpenMainProject(hDlg);
            return TRUE;
        }
        if (id == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}
