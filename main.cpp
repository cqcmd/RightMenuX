#include "common.h"
#include "features.h"
#include "resource.h"
#include "selftest.h"
#include <cstdio>
#include <cwchar>
#include <tlhelp32.h>   // Process32* 用于重启资源管理器

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// ============================================================
// main.cpp - RightMenuX v5.1
// v5.1：弃用 v5.0 的"单文件 EXE-as-COM-server"——带 requireAdministrator 清单的
//       exe 被 COM 拒绝在 Explorer 进程内加载（命令提示符不显示 / 显示隐藏无效的根因）。
//       改回混合架构：COM 服务器编译为独立 RightMenuXShell.dll（无提权清单），
//       由 RightMenuX.exe 以 RCDATA 内嵌，启用功能时释放到 C:\Windows 并注册。
//       管理器 exe 仍保留 requireAdministrator（写 HKLM 弹 UAC）。
// v5.0：项目更名「RightMenuX」。曾尝试彻底单文件化（_mcentry 双形态入口，见
//       explorercommand.cpp 存档），因上述 COM 加载限制而回退。
//       命令提示符 %V 占位符修复保留在 COM 服务器 Invoke 中。
// v4.4：架构合并（用户问"子功能 exe 能不能写成 dll"）：
//       原 右键增强切换.exe（SuperHidden 切换）合并进 右键增强菜单.dll
//       （新增 ToggleSuperHidden 导出，COM 服务器进程内调用，仍无 UAC）；
//       原 右键增强自检.exe 删除，自检逻辑本就内嵌于管理器（--selftest）。
// v4.3：体积优化——链接加 -s -Wl,--gc-sections，保持 -static 零依赖不变。
// v4.2：命令提示符改为仅注册到文件夹右键（不再注册空白处/背景右键）；
//       卸载时最佳努力清理历史版本残留的背景命令提示符键。
// v4.1：UI 打磨（CS_DROPSHADOW 原生阴影 / 命令提示符单一功能 / 自绘弹窗按钮修复 / 圆角标题栏）。
// v4.0：项目更名「右键增强」（原 WinPersonalize）。新增"在此打开命令提示符"右键功能。
//       现工具纯粹为右键菜单增强（RunCommand + ShellExtension）。
// 功能逻辑（拖拽、安装/卸载回退、ESC、DESTROY、--selftest）一律不动。
// ============================================================

#define MAX_FEATURES 32
#define WIN_W        600
#define TITLE_H      46
#define HEADER_H     96
#define CARD_H       116
#define CARD_GAP     14
#define FOOTER_H     64
#define MARGIN       28
#define TB_BTN_W     46
#define SHADOW_PAD   1       // 仅用于 1px 边框留白（不再绘制灰色外阴影）
#define WIN_RADIUS   12      // 窗口/卡片圆角半径

// ---- 设计 Token v3.4（在 v3.3 基础上调谐，整体更通透协调） ----
static const COLORREF C_BG              = RGB(244, 245, 248); // #F4F5F8 内容背景
static const COLORREF C_TB_BG           = RGB(248, 249, 251); // #F8F9FB 标题栏（略亮于内容，分层）
static const COLORREF C_CARD_BG         = RGB(255, 255, 255); // #FFFFFF
static const COLORREF C_BORDER          = RGB(197, 203, 213); // #C5CBD5 卡片常态边框(加深,清晰可辨)
static const COLORREF C_DIVIDER         = RGB(232, 235, 240); // #E8EBF0 极细分隔线
static const COLORREF C_TITLE           = RGB(26, 30, 38);    // #1A1E26 标题/英雄文案（近黑）
static const COLORREF C_TEXT            = RGB(38, 43, 51);    // #262B33 功能名
static const COLORREF C_SUBTITLE        = RGB(107, 114, 128); // #6B7280 副标题/说明
static const COLORREF C_DESC            = RGB(93, 103, 114);  // #5D6772 卡片描述（WCAG AA）
static const COLORREF C_PRIMARY         = RGB(37, 99, 235);   // #2563EB
static const COLORREF C_PRIMARY_H       = RGB(29, 78, 216);   // #1D4ED8
static const COLORREF C_GREEN           = RGB(22, 163, 74);   // #16A34A 图形强调（瓦片/彩条）
static const COLORREF C_GREEN_DARK      = RGB(21, 128, 61);   // #15803D 已启用徽章字色（WCAG AA）
static const COLORREF C_GREEN_BG        = RGB(223, 242, 230); // #DFF2E6 已启用底色
static const COLORREF C_AMBER           = RGB(202, 110, 4);   // #CA6E04 更深琥珀（白底可读性更好）
static const COLORREF C_AMBER_BG        = RGB(254, 244, 225); // #FEF4E1
static const COLORREF C_BTN_SECONDARY   = RGB(255, 255, 255);
static const COLORREF C_BTN_SECONDARY_H = RGB(247, 248, 251); // #F7F8FB
static const COLORREF C_BTN_BORDER      = RGB(209, 214, 222); // #D1D6DE
static const COLORREF C_BTN_BORDER_H    = RGB(197, 204, 214); // #C5CCD6 hover 时略深
static const COLORREF C_BADGE_NO_BG     = RGB(241, 243, 247); // #F1F3F7
static const COLORREF C_BADGE_NO_FG     = RGB(86, 96, 109);   // #56606D 未启用字色（WCAG AA）
static const COLORREF C_SHADOW          = RGB(188, 195, 208); // #BC9FD0 卡片阴影近层(加深,明显不脏)
static const COLORREF C_SHADOW_2        = RGB(213, 219, 228); // #D5DBE4 卡片阴影远层(浅)
static const COLORREF C_TB_HOVER        = RGB(229, 231, 236); // #E5E7EC 最小化按钮 hover
static const COLORREF C_TB_CLOSE_HOVER  = RGB(232, 17, 35);   // #E81123 关闭按钮 hover

// ---- 窗口层阴影（纯自绘，多层实色模拟柔和向下投影，收窄变淡） ----
static const COLORREF C_WIN_SHADOW_BASE = RGB(233, 236, 241); // #E9ECF1 四周极淡环境阴影
static const COLORREF C_WIN_SHADOW_3    = RGB(224, 229, 236); // #E0E5EC 投影最远层(最浅)
static const COLORREF C_WIN_SHADOW_2    = RGB(213, 219, 228); // #D5DBE4 投影中间层
static const COLORREF C_WIN_SHADOW_1    = RGB(199, 206, 218); // #C7CEDA 投影最近层(最深,贴内容底边)
static const COLORREF C_WIN_BORDER      = RGB(196, 202, 211); // #C4CAD3 窗口 1px 圆角外边框(清晰中性深)

// DWM 圆角（Win11 生效，Win10 忽略失败）
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static RECT  g_cardRect[MAX_FEATURES];
static RECT  g_btnRect[MAX_FEATURES];
static BOOL  g_hover[MAX_FEATURES];
static RECT  g_minBtnRect;
static RECT  g_closeBtnRect;
static BOOL  g_minHover = FALSE;
static BOOL  g_closeHover = FALSE;
static int   g_count = 0;
static int   g_totalH = 0;
static WCHAR g_osString[128] = {0};
static HICON g_hAppIcon = NULL; // 标题栏真实应用图标（LoadIcon）
static HINSTANCE g_hInst = NULL; // 全局实例句柄（供自绘弹窗注册类使用）

// ---- 自绘模态弹窗（替代原生 MessageBox，风格与主界面统一） ----
struct NoticeCtx {
    const WCHAR* caption;
    const WCHAR* body;
    BOOL         error;
    HWND         parent;
    int          dlgW, dlgH;
    RECT         btnRect;
    BOOL         btnHover;
};
static NoticeCtx g_notice = {0};
static const WCHAR* NOTICE_CLASS = L"WinNoticeDialog";

// 自绘模态提示框：圆角白底 + 信息/错误圆形图标 + 标题 + 自动换行正文 + 蓝色“确定”按钮。
// 风格与主界面卡片一致，避免原生 MessageBox 的突兀感。
static void ShowNotice(HWND parent, LPCWSTR cap, LPCWSTR body, BOOL error);

// ============================================================
// 绘图基元
// ============================================================

static void DrawRoundedRect(HDC hdc, RECT* r, int radius,
                            COLORREF fill, COLORREF border, int borderW = 1) {
    HPEN pen = CreatePen(PS_SOLID, borderW, border);
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN op = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, brush);
    RoundRect(hdc, r->left, r->top, r->right, r->bottom, radius * 2, radius * 2);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(pen);
    DeleteObject(brush);
}

// 仅描边（不填充）的圆角矩形——用于图标线稿（填充由彩色瓦片本身提供）
static void StrokeRoundedRect(HDC hdc, RECT* r, int radius,
                              COLORREF stroke, int w = 2) {
    HPEN pen = CreatePen(PS_SOLID, w, stroke);
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN op = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    RoundRect(hdc, r->left, r->top, r->right, r->bottom, radius * 2, radius * 2);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(pen);
}

// 仅描边的圆（用于"眼睛"等图标线稿）
static void StrokeEllipse(HDC hdc, int cx, int cy, int r,
                          COLORREF stroke, int w = 2) {
    HPEN pen = CreatePen(PS_SOLID, w, stroke);
    HBRUSH br = (HBRUSH)GetStockObject(NULL_BRUSH);
    HPEN op = (HPEN)SelectObject(hdc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, br);
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(pen);
}

// 统一字体工厂（避免重复样板，降低告警风险）
static HFONT MakeFont(int hpx, int weight) {
    return CreateFontW(-hpx, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
}

// 卡片：白色圆角 + 多层实色向下投影 + 左侧 3px 彩条 + hover 着主色边框
static void DrawCard(HDC hdc, RECT* r, BOOL hovered, COLORREF accent) {
    int rad = WIN_RADIUS;
    // 多层实色阴影：远层(浅,偏移大) -> 近层(深,偏移小)，向下投影。
    // GDI 无半透明，用多层实色模拟柔和层次，避免单层浅灰像"脏污"。
    RECT sh2 = {r->left + 1, r->top + 5, r->right + 1, r->bottom + 5};
    DrawRoundedRect(hdc, &sh2, rad, C_SHADOW_2, C_SHADOW_2);
    RECT sh1 = {r->left,     r->top + 3, r->right,     r->bottom + 3};
    DrawRoundedRect(hdc, &sh1, rad, C_SHADOW, C_SHADOW);
    // 卡片本体
    COLORREF brd = hovered ? C_PRIMARY : C_BORDER;
    int bw = hovered ? 2 : 1;
    DrawRoundedRect(hdc, r, rad, C_CARD_BG, brd, bw);
    // 左侧 3px 圆角类型彩条（绿=RunCommand / 琥珀=ShellExtension）
    RECT ar = {r->left, r->top + 14, r->left + 3, r->bottom - 14};
    DrawRoundedRect(hdc, &ar, 2, accent, accent);
}

// 按钮：圆角 8px；主操作（启用）蓝底白字，次操作（停用）白底描边
static void DrawButton(HDC hdc, RECT* r, const WCHAR* text,
                       BOOL hovered, BOOL primary) {
    COLORREF fill, border, textc;
    if (primary) {
        fill   = hovered ? C_PRIMARY_H : C_PRIMARY;
        border = fill;
        textc  = RGB(255, 255, 255);
    } else {
        fill   = hovered ? C_BTN_SECONDARY_H : C_BTN_SECONDARY;
        border = hovered ? C_BTN_BORDER_H : C_BTN_BORDER;
        textc  = C_TEXT;
    }
    DrawRoundedRect(hdc, r, 8, fill, border);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textc);
    HFONT f = MakeFont(13, FW_MEDIUM);
    HFONT of = (HFONT)SelectObject(hdc, f);
    DrawTextW(hdc, text, -1, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, of);
    DeleteObject(f);
}

// 功能图标：彩色圆角瓦片（绿/琥珀）+ 白色线稿语义图形
static void DrawFeatureIcon(HDC hdc, RECT* r, const WCHAR* fid, COLORREF accent) {
    (void)accent; // 瓦片底色由调用处填充，这里只画线稿
    int cx = (r->left + r->right) / 2, cy = (r->top + r->bottom) / 2;

    if (wcscmp(fid, L"ComputerManagement") == 0) {
        // 显示器
        RECT s = {cx - 13, cy - 10, cx + 13, cy + 2};
        StrokeRoundedRect(hdc, &s, 3, RGB(255, 255, 255), 2);
        HPEN wp = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN owp = (HPEN)SelectObject(hdc, wp);
        MoveToEx(hdc, cx, cy + 2, NULL);  LineTo(hdc, cx, cy + 7);
        MoveToEx(hdc, cx - 7, cy + 7, NULL); LineTo(hdc, cx + 7, cy + 7);
        SelectObject(hdc, owp);
        DeleteObject(wp);
    } else if (wcscmp(fid, L"DeviceManager") == 0) {
        // 芯片
        RECT sq = {cx - 9, cy - 9, cx + 9, cy + 9};
        StrokeRoundedRect(hdc, &sq, 2, RGB(255, 255, 255), 2);
        RECT dot = {cx - 3, cy - 3, cx + 3, cy + 3};
        HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
        HBRUSH owb = (HBRUSH)SelectObject(hdc, wb);
        Rectangle(hdc, dot.left, dot.top, dot.right, dot.bottom);
        SelectObject(hdc, owb);
        DeleteObject(wb);
        HPEN wp = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN owp = (HPEN)SelectObject(hdc, wp);
        for (int o = -6; o <= 6; o += 6) {
            MoveToEx(hdc, cx + o, cy - 9, NULL);  LineTo(hdc, cx + o, cy - 12);
            MoveToEx(hdc, cx + o, cy + 9, NULL);  LineTo(hdc, cx + o, cy + 12);
            MoveToEx(hdc, cx - 9, cy + o, NULL);  LineTo(hdc, cx - 12, cy + o);
            MoveToEx(hdc, cx + 9, cy + o, NULL);  LineTo(hdc, cx + 12, cy + o);
        }
        SelectObject(hdc, owp);
        DeleteObject(wp);
    } else if (wcscmp(fid, L"CmdHere") == 0) {
        // 命令提示符：终端窗口 + ">" 提示符
        RECT term = {cx - 14, cy - 11, cx + 14, cy + 11};
        StrokeRoundedRect(hdc, &term, 3, RGB(255, 255, 255), 2);
        // ">" 提示符
        HPEN wp = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN owp = (HPEN)SelectObject(hdc, wp);
        MoveToEx(hdc, cx - 9, cy - 3, NULL); LineTo(hdc, cx - 4, cy + 1);
        MoveToEx(hdc, cx - 4, cy + 1, NULL);  LineTo(hdc, cx - 9, cy + 5);
        SelectObject(hdc, owp);
        DeleteObject(wp);
        // 光标短横
        HPEN cp = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN ocp = (HPEN)SelectObject(hdc, cp);
        MoveToEx(hdc, cx - 1, cy + 3, NULL); LineTo(hdc, cx + 6, cy + 3);
        SelectObject(hdc, ocp);
        DeleteObject(cp);
    } else {
        // 文件夹 + 眼睛（显示/隐藏）
        RECT fb = {cx - 14, cy - 5, cx + 14, cy + 11};
        StrokeRoundedRect(hdc, &fb, 3, RGB(255, 255, 255), 2);
        RECT ft = {cx - 14, cy - 10, cx - 2, cy - 5};
        HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
        HBRUSH owb = (HBRUSH)SelectObject(hdc, wb);
        RoundRect(hdc, ft.left, ft.top, ft.right, ft.bottom, 2 * 2, 2 * 2);
        SelectObject(hdc, owb);
        DeleteObject(wb);
        int ex = cx, ey = cy + 3, er = 4;
        StrokeEllipse(hdc, ex, ey, er, RGB(255, 255, 255), 2);
        HBRUSH pb = CreateSolidBrush(RGB(255, 255, 255));
        HBRUSH opb = (HBRUSH)SelectObject(hdc, pb);
        Ellipse(hdc, ex - 1, ey - 1, ex + 1, ey + 1);
        SelectObject(hdc, opb);
        DeleteObject(pb);
    }
}

// 状态药丸徽章 + 左侧指示圆点
static void DrawStatusBadge(HDC hdc, RECT* r, BOOL installed) {
    COLORREF bg = installed ? C_GREEN_BG : C_BADGE_NO_BG;
    COLORREF fg = installed ? C_GREEN_DARK : C_BADGE_NO_FG;
    DrawRoundedRect(hdc, r, 11, bg, bg);
    int cy = (r->top + r->bottom) / 2;
    int dr = 3;
    RECT dot = {r->left + 10, cy - dr, r->left + 10 + 2 * dr, cy + dr};
    HBRUSH b = CreateSolidBrush(installed ? fg : bg);
    HPEN p = CreatePen(PS_SOLID, 1, fg);
    HPEN op = (HPEN)SelectObject(hdc, p);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
    Ellipse(hdc, dot.left, dot.top, dot.right, dot.bottom);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(p);
    DeleteObject(b);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    HFONT f = MakeFont(11, FW_MEDIUM);
    HFONT of = (HFONT)SelectObject(hdc, f);
    RECT tR = {r->left + 22, r->top, r->right - 8, r->bottom};
    DrawTextW(hdc, installed ? L"\u5df2\u542f\u7528" : L"\u672a\u542f\u7528",
              -1, &tR, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, of);
    DeleteObject(f);
}

// 标题栏按钮：圆角背景（hover 高亮）+ 细线字形（minimize 横线 / close X）
static void DrawTitleButton(HDC hdc, RECT* r, BOOL hover, BOOL close) {
    if (hover) {
        COLORREF fill = close ? C_TB_CLOSE_HOVER : C_TB_HOVER;
        DrawRoundedRect(hdc, r, 6, fill, fill);
    }
    int cx = (r->left + r->right) / 2, cy = (r->top + r->bottom) / 2;
    COLORREF c = hover ? (close ? RGB(255, 255, 255) : C_SUBTITLE) : C_SUBTITLE;
    HPEN p = CreatePen(PS_SOLID, 1, c);
    HPEN op = (HPEN)SelectObject(hdc, p);
    if (close) {
        MoveToEx(hdc, cx - 5, cy - 5, NULL); LineTo(hdc, cx + 5, cy + 5);
        MoveToEx(hdc, cx - 5, cy + 5, NULL); LineTo(hdc, cx + 5, cy - 5);
    } else {
        MoveToEx(hdc, cx - 5, cy, NULL); LineTo(hdc, cx + 5, cy);
    }
    SelectObject(hdc, op);
    DeleteObject(p);
}

// ============================================================
// 自绘模态弹窗（替代原生 MessageBox，风格与主界面统一）
// ============================================================
static LRESULT CALLBACK NoticeProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        int W = 380, P = 24, iconSz = 36, gap = 14;
        int bodyW = W - 2 * P;
        // 计算正文高度（自动换行）
        HDC hdc = GetDC(hWnd);
        HFONT bf = MakeFont(13, FW_NORMAL);
        HFONT of = (HFONT)SelectObject(hdc, bf);
        RECT r = {0, 0, bodyW, 0};
        DrawTextW(hdc, g_notice.body, -1, &r, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
        int bodyH = r.bottom - r.top;
        SelectObject(hdc, of); DeleteObject(bf); ReleaseDC(hWnd, hdc);

        int btnW = 96, btnH = 34, btnGap = 18;
        int H = P + iconSz + gap + bodyH + btnGap + btnH + P;
        g_notice.dlgW = W; g_notice.dlgH = H;
        SetWindowPos(hWnd, NULL, 0, 0, W, H, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

        // 确定按钮：右下角对齐
        g_notice.btnRect.left   = W - P - btnW;
        g_notice.btnRect.top    = H - P - btnH;
        g_notice.btnRect.right  = W - P;
        g_notice.btnRect.bottom = H - P;

        // 居中于父窗口（若有）
        if (g_notice.parent) {
            RECT pr; GetWindowRect(g_notice.parent, &pr);
            int px = (pr.left + pr.right) / 2 - W / 2;
            int py = (pr.top + pr.bottom) / 2 - H / 2;
            SetWindowPos(hWnd, NULL, px, py, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        // Win11 DWM 圆角
        HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            typedef HRESULT (WINAPI *DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
            DwmSetWindowAttribute_t p = (DwmSetWindowAttribute_t)GetProcAddress(hDwm, "DwmSetWindowAttribute");
            if (p) { DWORD c = DWMWCP_ROUND; p(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &c, sizeof(c)); }
            FreeLibrary(hDwm);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hWnd, &ps);
        int W = g_notice.dlgW, H = g_notice.dlgH, P = 24, iconSz = 36;

        // 内容圆角白底 + 细边框（无外阴影，简洁）
        RECT fullR = {0, 0, W, H};
        DrawRoundedRect(hdc, &fullR, 10, C_CARD_BG, C_WIN_BORDER);

        // 圆形图标（信息蓝 / 错误红）
        int ix = P, iy = P;
        COLORREF ic = g_notice.error ? RGB(220, 38, 38) : C_PRIMARY;
        HBRUSH ib = CreateSolidBrush(ic);
        HPEN ip = CreatePen(PS_SOLID, 1, ic);
        HPEN oip = (HPEN)SelectObject(hdc, ip);
        HBRUSH oib = (HBRUSH)SelectObject(hdc, ib);
        Ellipse(hdc, ix, iy, ix + iconSz, iy + iconSz);
        SelectObject(hdc, oip); SelectObject(hdc, oib);
        DeleteObject(ip); DeleteObject(ib);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT sf = MakeFont(20, FW_BOLD);
        HFONT osf = (HFONT)SelectObject(hdc, sf);
        RECT sr = {ix, iy, ix + iconSz, iy + iconSz};
        DrawTextW(hdc, g_notice.error ? L"!" : L"i", -1, &sr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, osf); DeleteObject(sf);

        // 标题（图标右侧垂直居中）
        HFONT cf = MakeFont(15, FW_SEMIBOLD);
        HFONT ocf = (HFONT)SelectObject(hdc, cf);
        SetTextColor(hdc, C_TITLE);
        RECT cr = {ix + iconSz + 12, iy, W - P, iy + iconSz};
        DrawTextW(hdc, g_notice.caption, -1, &cr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, ocf); DeleteObject(cf);

        // 正文（图标下自动换行）
        int bodyTop = iy + iconSz + 14;
        HFONT bf = MakeFont(13, FW_NORMAL);
        HFONT obf = (HFONT)SelectObject(hdc, bf);
        SetTextColor(hdc, C_TEXT);
        RECT br = {P, bodyTop, W - P, H - P - 34 - 18};
        DrawTextW(hdc, g_notice.body, -1, &br, DT_LEFT | DT_WORDBREAK | DT_VCENTER);
        SelectObject(hdc, obf); DeleteObject(bf);

        // 确定按钮
        DrawButton(hdc, &g_notice.btnRect, L"\u786e\u5b9a", g_notice.btnHover, TRUE);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        BOOL h = PtInRect(&g_notice.btnRect, pt);
        if (h != g_notice.btnHover) { g_notice.btnHover = h; InvalidateRect(hWnd, NULL, FALSE); }
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_notice.btnHover) { g_notice.btnHover = FALSE; InvalidateRect(hWnd, NULL, FALSE); }
        return 0;

    case WM_LBUTTONDOWN: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        if (PtInRect(&g_notice.btnRect, pt)) DestroyWindow(hWnd);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_RETURN || wParam == VK_ESCAPE) DestroyWindow(hWnd);
        return 0;
    case WM_NCHITTEST: {
        // 按钮区域返回 HTCLIENT：否则 WM_NCHITTEST 整体返回 HTCAPTION 会被当成
        // 标题栏拖拽（WM_NCLBUTTONDOWN），导致"确定"按钮的 WM_LBUTTONDOWN 永不触发。
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hWnd, &pt);
        if (PtInRect(&g_notice.btnRect, pt)) return HTCLIENT;
        return HTCAPTION; // 其余区域允许拖动
    }
    case WM_DESTROY:
        // 注意：自绘弹窗销毁时不要 PostQuitMessage，否则会误退出主窗口循环；
        // 模态循环依据 IsWindow(h) 判断是否结束。
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// 模态显示自绘提示框（替代 MessageBox）。owner 窗口由系统自动禁用/启用。
static void ShowNotice(HWND parent, LPCWSTR cap, LPCWSTR body, BOOL error) {
    g_notice.caption  = cap;
    g_notice.body     = body;
    g_notice.error    = error;
    g_notice.parent   = parent;
    g_notice.btnHover = FALSE;
    HWND h = CreateWindowExW(0, NOTICE_CLASS, L"",
        WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 380, 200,
        parent, NULL, g_hInst, NULL);
    if (!h) return;
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!IsWindow(h)) break;   // 弹窗已销毁则退出模态循环
    }
}

// 窗口层面：纯自绘圆角白底 + 1px 清晰外边框。
// 不再绘制灰色外阴影（原 SHADOW_PAD 8px 灰边让用户觉得"四周都有阴影很别扭"），
// 柔和投影改由窗口类的 CS_DROPSHADOW（系统原生、轻量）提供。
static void DrawWindowFrame(HDC hdc, int W, int H) {
    RECT full = {0, 0, W, H};
    HBRUSH ob = CreateSolidBrush(C_BG);
    FillRect(hdc, &full, ob);
    DeleteObject(ob);
}

// ============================================================
// 布局
// ============================================================

static void CenterWindow(HWND hWnd) {
    RECT s; GetWindowRect(GetDesktopWindow(), &s);
    RECT w; GetWindowRect(hWnd, &w);
    SetWindowPos(hWnd, NULL,
        (s.right - s.left - (w.right - w.left)) / 2,
        (s.bottom - s.top - (w.bottom - w.top)) / 2,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// 布局
static void ComputeLayout() {
    g_count = (int)g_featureCount;
    // 所有坐标均为“客户端坐标”：内容区整体内缩 SHADOW_PAD，
    // 以便窗口四周留出阴影边距（与 WM_PAINT 的纯自绘投影一致，命中测试也用此坐标）。
    for (int i = 0; i < g_count; i++) {
        int y = TITLE_H + HEADER_H + CARD_GAP + i * (CARD_H + CARD_GAP);
        g_cardRect[i] = {MARGIN + SHADOW_PAD, y + SHADOW_PAD,
                         WIN_W - MARGIN + SHADOW_PAD, y + CARD_H + SHADOW_PAD};
        // 按钮在卡片内垂直居中、靠右
        int bw = 84, bh = 32;
        int by = y + (CARD_H - bh) / 2;
        g_btnRect[i] = {WIN_W - MARGIN - 18 - bw + SHADOW_PAD, by + SHADOW_PAD,
                        WIN_W - MARGIN - 18 + SHADOW_PAD, by + bh + SHADOW_PAD};
        g_hover[i] = FALSE;
    }
    g_totalH = TITLE_H + HEADER_H + CARD_GAP + g_count * (CARD_H + CARD_GAP) + FOOTER_H;

    g_minBtnRect  = {WIN_W + SHADOW_PAD - 2 * TB_BTN_W, SHADOW_PAD,
                     WIN_W + SHADOW_PAD - TB_BTN_W, TITLE_H + SHADOW_PAD};
    g_closeBtnRect = {WIN_W + SHADOW_PAD - TB_BTN_W, SHADOW_PAD,
                      WIN_W + SHADOW_PAD, TITLE_H + SHADOW_PAD};
}

// ============================================================
// 窗口过程
// ============================================================

static LRESULT CALLBACK DialogProc(HWND hWnd, UINT uMsg,
                                   WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        GetOSDisplayString(g_osString, _countof(g_osString));
        // 真实应用图标（LoadIcon 共享句柄，无需 DestroyIcon）
        g_hAppIcon = LoadIconW(((LPCREATESTRUCTW)lParam)->hInstance,
                               MAKEINTRESOURCE(IDI_APP_ICON));
        ComputeLayout();

        // 纯 WS_POPUP 无系统边框：客户区即窗口区。窗口尺寸在内容尺寸之外
        // 每边多留 SHADOW_PAD，用于纯自绘柔和投影（不再依赖 DWM 阴影）。
        SetWindowPos(hWnd, NULL, 0, 0, WIN_W + 2 * SHADOW_PAD, g_totalH + 2 * SHADOW_PAD,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
        CenterWindow(hWnd);

        // Win11 DWM 圆角（Win10 上忽略失败）
        HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
        if (hDwm) {
            typedef HRESULT (WINAPI *DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
            DwmSetWindowAttribute_t p = (DwmSetWindowAttribute_t)GetProcAddress(hDwm, "DwmSetWindowAttribute");
            if (p) {
                DWORD corner = DWMWCP_ROUND;
                p(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
            }
            FreeLibrary(hDwm);
        }
        return 0;
    }

    // 纯 WS_POPUP 无系统非客户区，非客户区本就为零，无需 WM_NCCALCSIZE。
    // （旧版用 WS_THICKFRAME + 清零非客户区去白边并保 DWM 阴影，
    //   但清零后 DWM 阴影反而不可见；现改为纯自绘投影，故移除该处理。）

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT cr; GetClientRect(hWnd, &cr);
        int W = cr.right - cr.left, H = cr.bottom - cr.top;

        // 窗口层：纯自绘柔和向下投影 + 内容区底色（先于内容绘制）
        DrawWindowFrame(hdc, W, H);

        // 背景（内容区，已内缩 SHADOW_PAD）
        HBRUSH bg = CreateSolidBrush(C_BG);
        RECT cbody = {SHADOW_PAD, SHADOW_PAD, WIN_W + SHADOW_PAD, g_totalH + SHADOW_PAD};
        FillRect(hdc, &cbody, bg);
        DeleteObject(bg);

        // ====== 自定义标题栏 ======
        {
            RECT tb = {SHADOW_PAD, SHADOW_PAD, WIN_W + SHADOW_PAD, TITLE_H + SHADOW_PAD};
            HBRUSH tbb = CreateSolidBrush(C_TB_BG);
            FillRect(hdc, &tb, tbb);
            DeleteObject(tbb);

            // 标题栏按钮（圆角，hover 高亮）
            DrawTitleButton(hdc, &g_minBtnRect, g_minHover, FALSE);
            DrawTitleButton(hdc, &g_closeBtnRect, g_closeHover, TRUE);

            // 底部分隔线（极细，衔接标题栏与内容）
            HPEN sp = CreatePen(PS_SOLID, 1, C_DIVIDER);
            HPEN osp = (HPEN)SelectObject(hdc, sp);
            MoveToEx(hdc, SHADOW_PAD, TITLE_H + SHADOW_PAD, NULL);
            LineTo(hdc, WIN_W + SHADOW_PAD, TITLE_H + SHADOW_PAD);
            SelectObject(hdc, osp);
            DeleteObject(sp);

            // 左侧真实应用图标（失败则回退为品牌色方块）
            int iSz = 22;
            int ix = 16 + SHADOW_PAD, iy = SHADOW_PAD + (TITLE_H - iSz) / 2;
            if (g_hAppIcon) {
                DrawIconEx(hdc, ix, iy, g_hAppIcon, iSz, iSz, 0, NULL, DI_NORMAL);
            } else {
                RECT ir = {ix, iy, ix + iSz, iy + iSz};
                DrawRoundedRect(hdc, &ir, 5, C_PRIMARY, C_PRIMARY);
            }

            // 标题文字（与图标同高，垂直居中）
            HFONT tf = MakeFont(14, FW_SEMIBOLD);
            HFONT of = (HFONT)SelectObject(hdc, tf);
            SetTextColor(hdc, C_TITLE);
            SetBkMode(hdc, TRANSPARENT);
            RECT tR = {ix + iSz + 10, SHADOW_PAD, WIN_W + SHADOW_PAD - 2 * TB_BTN_W, TITLE_H + SHADOW_PAD};
            DrawTextW(hdc, L"RightMenuX", -1, &tR,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, of);
            DeleteObject(tf);

            // 窗口控件字形由 DrawTitleButton 绘制（见上方调用）
        }

        // ====== 导语区（替代重复大标题，提供信息层次） ======
        {
            int heroY = TITLE_H + 20 + SHADOW_PAD;
            HFONT heroF = MakeFont(16, FW_SEMIBOLD);
            HFONT of = (HFONT)SelectObject(hdc, heroF);
            SetTextColor(hdc, C_TITLE);
            SetBkMode(hdc, TRANSPARENT);
            RECT hR = {MARGIN + SHADOW_PAD, heroY, WIN_W - MARGIN + SHADOW_PAD, heroY + 26};
            DrawTextW(hdc, L"\u53f3\u952e\u83dc\u5355\u589e\u5f3a\u5de5\u5177", -1, &hR,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, of);
            DeleteObject(heroF);

            // 品牌色短装饰线
            HPEN ap = CreatePen(PS_SOLID, 3, C_PRIMARY);
            HPEN aop = (HPEN)SelectObject(hdc, ap);
            int lineY = TITLE_H + 20 + 26 + SHADOW_PAD;
            MoveToEx(hdc, MARGIN + SHADOW_PAD, lineY, NULL);
            LineTo(hdc, MARGIN + 28 + SHADOW_PAD, lineY);
            SelectObject(hdc, aop);
            DeleteObject(ap);

            // OS 信息副标题
            int subY = TITLE_H + 50 + SHADOW_PAD;
            HFONT subF = MakeFont(13, FW_NORMAL);
            HFONT osf = (HFONT)SelectObject(hdc, subF);
            SetTextColor(hdc, C_SUBTITLE);
            SetBkMode(hdc, TRANSPARENT);
            RECT sR = {MARGIN + SHADOW_PAD, subY, WIN_W - MARGIN + SHADOW_PAD, subY + 22};
            DrawTextW(hdc, g_osString, -1, &sR, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, osf);
            DeleteObject(subF);
        }

        // 导语区与卡片列表之间的分隔线
        {
            HPEN line = CreatePen(PS_SOLID, 1, C_DIVIDER);
            HPEN ol = (HPEN)SelectObject(hdc, line);
            int divY = TITLE_H + HEADER_H - 16 + SHADOW_PAD;
            MoveToEx(hdc, MARGIN + SHADOW_PAD, divY, NULL);
            LineTo(hdc, WIN_W - MARGIN + SHADOW_PAD, divY);
            SelectObject(hdc, ol);
            DeleteObject(line);
        }

        // ====== 卡片列表 ======
        for (int i = 0; i < g_count; i++) {
            const Feature* f = &g_features[i];
            BOOL installed = Feature_IsInstalled(f);
            COLORREF accent = (f->kind == FeatureKind::RunCommand) ? C_GREEN : C_AMBER;

            DrawCard(hdc, &g_cardRect[i], g_hover[i], accent);

            // 图标瓦片（彩色圆角方块 + 白色线稿）
            int iconSz = 46;
            RECT ir = {g_cardRect[i].left + 18,
                       g_cardRect[i].top + (CARD_H - iconSz) / 2,
                       g_cardRect[i].left + 18 + iconSz,
                       g_cardRect[i].top + (CARD_H - iconSz) / 2 + iconSz};
            DrawRoundedRect(hdc, &ir, 11, accent, accent);
            DrawFeatureIcon(hdc, &ir, f->id, accent);

            // 文字起始 X
            int tx = g_cardRect[i].left + 18 + iconSz + 16;

            // 功能名称
            {
                HFONT nameF = MakeFont(16, FW_SEMIBOLD);
                HFONT of = (HFONT)SelectObject(hdc, nameF);
                SetTextColor(hdc, C_TEXT);
                SetBkMode(hdc, TRANSPARENT);
                RECT nR = {tx, g_cardRect[i].top + 18, g_btnRect[i].left - 14,
                           g_cardRect[i].top + 18 + 24};
                DrawTextW(hdc, f->displayName, -1, &nR,
                          DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                SelectObject(hdc, of);
                DeleteObject(nameF);
            }

            // 说明文字（两行完整显示，超长才省略，不再单行截断）
            {
                HFONT descF = MakeFont(15, FW_NORMAL);
                HFONT of = (HFONT)SelectObject(hdc, descF);
                SetTextColor(hdc, C_DESC);
                SetBkMode(hdc, TRANSPARENT);
                RECT dR = {tx, g_cardRect[i].top + 42, g_btnRect[i].left - 14,
                           g_cardRect[i].top + 42 + 40};
                DrawTextW(hdc, f->description, -1, &dR,
                          DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_VCENTER);
                SelectObject(hdc, of);
                DeleteObject(descF);
            }

            // 状态徽章（带指示圆点，卡片底部一行）
            {
                int bW = 76, bH = 22;
                RECT stR = {tx, g_cardRect[i].top + 88, tx + bW, g_cardRect[i].top + 88 + bH};
                DrawStatusBadge(hdc, &stR, installed);
            }

            // 操作按钮（卡片内右侧垂直居中）
            DrawButton(hdc, &g_btnRect[i],
                       installed ? L"\u505c\u7528" : L"\u542f\u7528",
                       g_hover[i], !installed);
        }

        // ====== 页脚（两行：提示略大 + 版权信息） ======
        {
            int zoneTop = g_totalH - FOOTER_H + SHADOW_PAD;
            int zoneH = FOOTER_H;
            int cyMid = zoneTop + zoneH / 2;
            // 提示行（字号略增：11 -> 13）
            HFONT footF = MakeFont(14, FW_NORMAL);
            HFONT of = (HFONT)SelectObject(hdc, footF);
            SetTextColor(hdc, C_SUBTITLE);
            SetBkMode(hdc, TRANSPARENT);
            RECT l1 = {MARGIN + SHADOW_PAD, cyMid - 18, WIN_W - MARGIN + SHADOW_PAD, cyMid};
            DrawTextW(hdc,
                L"\u53f3\u952e\u83dc\u5355\u9879\u542f\u7528\u540e\u5373\u65f6\u751f\u6548\uff0c\u65e0\u9700\u91cd\u542f",
                -1, &l1, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, of);
            DeleteObject(footF);

            // 版权信息行（与 app.rc 的 CompanyName / LegalCopyright 保持一致）
            HFONT copyF = MakeFont(13, FW_NORMAL);
            HFONT of2 = (HFONT)SelectObject(hdc, copyF);
            SetTextColor(hdc, C_SUBTITLE);
            SetBkMode(hdc, TRANSPARENT);
            RECT l2 = {MARGIN + SHADOW_PAD, cyMid + 1, WIN_W - MARGIN + SHADOW_PAD, cyMid + 19};
            WCHAR verBuf[64] = {0};
            LoadStringW(g_hInst, IDS_VERSION, verBuf, _countof(verBuf));
            WCHAR copyLine[128];
            swprintf(copyLine, _countof(copyLine),
                L"\u00a9 \u91cd\u5e86\u6301\u739b\u591a\u7f51\u7edc\u79d1\u6280\u6709\u9650\u516c\u53f8 \u00b7 Z.W.  v%s", verBuf);
            DrawTextW(hdc, copyLine, -1, &l2, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            SelectObject(hdc, of2);
            DeleteObject(copyF);
        }

        // 窗口 1px 清晰圆角外边框（最后描边，压在内容之上，不被底色覆盖）
        {
            RECT wf = {SHADOW_PAD, SHADOW_PAD, W - SHADOW_PAD, H - SHADOW_PAD};
            StrokeRoundedRect(hdc, &wf, WIN_RADIUS, C_WIN_BORDER, 1);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hWnd, &pt);
        RECT rc; GetClientRect(hWnd, &rc);
        if (pt.x < 0 || pt.x > rc.right || pt.y < 0 || pt.y > rc.bottom)
            return HTNOWHERE;

        // 固定尺寸工具窗口，禁止任何缩放。
        // 仅标题栏非按钮区域返回 HTCAPTION 用于拖拽，其余一律 HTCLIENT。
        if (pt.y < TITLE_H + SHADOW_PAD) {
            if (PtInRect(&g_closeBtnRect, pt) || PtInRect(&g_minBtnRect, pt))
                return HTCLIENT;
            return HTCAPTION;
        }
        return HTCLIENT;
    }

    case WM_MOUSEMOVE: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        BOOL changed = FALSE;
        for (int i = 0; i < g_count; i++) {
            BOOL h = PtInRect(&g_btnRect[i], pt);
            if (h != g_hover[i]) { g_hover[i] = h; changed = TRUE; }
        }
        BOOL minH = PtInRect(&g_minBtnRect, pt);
        if (minH != g_minHover) { g_minHover = minH; changed = TRUE; }
        BOOL closeH = PtInRect(&g_closeBtnRect, pt);
        if (closeH != g_closeHover) { g_closeHover = closeH; changed = TRUE; }
        if (changed) InvalidateRect(hWnd, NULL, FALSE);
        TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hWnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE: {
        BOOL changed = FALSE;
        for (int i = 0; i < g_count; i++) {
            if (g_hover[i]) { g_hover[i] = FALSE; changed = TRUE; }
        }
        if (g_minHover) { g_minHover = FALSE; changed = TRUE; }
        if (g_closeHover) { g_closeHover = FALSE; changed = TRUE; }
        if (changed) InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        if (PtInRect(&g_closeBtnRect, pt)) {
            PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (PtInRect(&g_minBtnRect, pt)) {
            ShowWindow(hWnd, SW_MINIMIZE);
            return 0;
        }
        for (int i = 0; i < g_count; i++) {
            if (PtInRect(&g_btnRect[i], pt)) {
                const Feature* f = &g_features[i];
                BOOL installed = Feature_IsInstalled(f);

                // —— 右键菜单类（RunCommand / ShellExtension） ——
                if (installed) {
                    BOOL a = Feature_Uninstall(f, HKEY_CLASSES_ROOT);
                    BOOL b = Feature_Uninstall(f, HKEY_CURRENT_USER);
                    if (a || b) {
                        ShowNotice(hWnd,
                            L"RightMenuX",
                            L"\u5df2\u505c\u7528\uff0c\u53f3\u952e\u83dc\u5355\u9879\u5df2\u79fb\u9664\u3002",
                            FALSE);
                    } else {
                        ShowNotice(hWnd,
                            L"RightMenuX",
                            L"\u505c\u7528\u5931\u8d25\uff1a\u6ce8\u518c\u8868\u5220\u9664\u88ab\u62d2\u7edd\u3002\n"
                            L"\u8bf7\u53f3\u952e\u201c\u4ee5\u7ba1\u7406\u5458\u8eab\u4efd\u8fd0\u884c\u201d\u672c\u7a0b\u5e8f\u540e\u91cd\u8bd5\u3002",
                            TRUE);
                    }
                } else {
                    BOOL ok = Feature_Install(f, HKEY_CLASSES_ROOT);
                    if (ok) {
                        ShowNotice(hWnd,
                            L"RightMenuX",
                            L"\u5df2\u542f\u7528\uff08\u5bf9\u672c\u673a\u6240\u6709\u7528\u6237\u751f\u6548\uff09"
                            L"\uff0c\u5728\u201c\u6b64\u7535\u8111\u201d\u53f3\u952e\u83dc\u5355\u4e2d\u5373\u53ef\u770b\u5230\u3002",
                            FALSE);
                    } else if (Feature_Install(f, HKEY_CURRENT_USER)) {
                        ShowNotice(hWnd,
                            L"RightMenuX",
                            L"\u5df2\u4e3a\u5f53\u524d\u7528\u6237\u542f\u7528"
                            L"\uff08\u672a\u83b7\u7ba1\u7406\u5458\u6743\u9650\uff0c\u4ec5\u5f53\u524d\u8d26\u6237\u751f\u6548\uff09"
                            L"\u3002\n\u5982\u9700\u5bf9\u6240\u6709\u7528\u6237\u751f\u6548\uff0c"
                            L"\u8bf7\u53f3\u952e\u201c\u4ee5\u7ba1\u7406\u5458\u8eab\u4ef5\u8fd0\u884c\u201d\u672c\u7a0b\u5e8f\u3002",
                            FALSE);
                    } else {
                        ShowNotice(hWnd,
                            L"RightMenuX",
                            L"\u542f\u7528\u5931\u8d25\uff1a\u6ce8\u518c\u8868\u5199\u5165\u88ab\u62d2\u7edd\u3002\n"
                            L"\u8bf7\u53f3\u952e\u201c\u4ee5\u7ba1\u7406\u5458\u8eab\u4efd\u8fd0\u884c\u201d\u672c\u7a0b\u5e8f\u540e\u91cd\u8bd5\u3002",
                            TRUE);
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
                break;
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ============================================================
// 入口
// ============================================================

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // 注册自绘弹窗类（供 ShowNotice 使用，含自检模式）
    WNDCLASSEXW nwc = {sizeof(nwc)};
    nwc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    nwc.lpfnWndProc = NoticeProc;
    nwc.hInstance = hInstance;
    nwc.hCursor = LoadCursor(NULL, IDC_ARROW);
    nwc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    nwc.lpszClassName = NOTICE_CLASS;
    RegisterClassExW(&nwc);

    // 自检模式
    if (lpCmdLine && wcsstr(lpCmdLine, L"--selftest")) {
        if (AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
        }
        int rc = SelfTestMain();
        // --selftest-nogui：自动化/无头运行，跳过结尾弹窗直接退出（报告已写入文件）
        if (!wcsstr(lpCmdLine, L"--selftest-nogui")) {
            WCHAR msg[320];
            swprintf(msg, _countof(msg),
                     L"\u81ea\u68c0\u5b8c\u6210\n\u901a\u8fc7\uff1a%d    \u5931\u8d25\uff1a%d\n\n"
                     L"\u5b8c\u6574\u65e5\u5fd7\u89c1 RightMenuX \u76ee\u5f55\u4e0b\u7684 selftest_report.txt",
                     SelfTestPass, SelfTestFail);
            ShowNotice(NULL, L"RightMenuX \u81ea\u68c0", msg, rc != 0);
        }
        return rc;
    }

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DROPSHADOW;
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"RightMenuXDialog";

    if (!RegisterClassExW(&wc)) return 1;

    // 纯 WS_POPUP：彻底无系统尺寸边框、无白边、无缩放。
    // 阴影与 1px 边框全部由 WM_PAINT 纯自绘（见 DrawWindowFrame）。
    HWND hWnd = CreateWindowExW(0, L"RightMenuXDialog",
        L"RightMenuX",
        WS_POPUP | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_W + 2 * SHADOW_PAD, 400,
        NULL, NULL, hInstance, NULL);
    if (!hWnd) return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
