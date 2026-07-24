# Changelog

## v1.0.2（v5.1.5）— 2026-07-24
- 项目文档全面对齐 RightMenuX 现状：重写 `PROJECT_ANALYSIS.md`、`ROADMAP.md`（弃用旧名 WinPersonalize 与旧架构描述），更新 `README.md`（补全功能表、四段式版本说明）、`DESIGN_SYSTEM.md`（标题与字号注释）。
- 弃用并删除历史参考项目 `SuperHiddenApp/`（功能已并入 RightMenuX 的 SuperHidden 模块）。
- 清理项目目录：移除构建中间产物（res.o / shellres.o / selftest_report.txt / RightMenuXShell.dll）与旧发布包 `RightMenuX_v1.0.0.19.zip`，以及项目根残留文件。
- 版本号 bump 到 `1.0.2.20260724`（四段式），发布包 `RightMenuX_v1.0.2.zip`。

## v1.0.1（v5.1.4）— 2026-07-24
- 版本号策略改进（用户决策）：
  - 改为四段式 `主.次.修订.编译日期戳(YYYYMMDD)`。
  - 前三段每次编译 修订号+1、满10进1（次/主随之进位），保留“编译推进”语义且位数受限，不再无限累加成 1.0.0.100。
  - 第四段为编译当天日期戳，直观可追溯是哪天编的；Windows `FILEVERSION` 数值段用 `YYMMDD`（<65535）防 16 位溢出，字符串用完整 `YYYYMMDD`。
  - `version_stamp.py` 重写：默认每次编译自动递增前三段+写入日期戳；`--set X.Y.Z` / `--bump patch|minor|major` 仍可手动发版。
  - 主界面底部版权行通过 `LoadString(IDS_VERSION)` 显示版本号（如 `© 重庆持玛多网络科技有限公司 · Z.W.  v1.0.1.20260724`）；`app.rc` 新增 `STRINGTABLE` / `IDS_VERSION=102`，`resource.h` 同步定义。

## v1.0.0.19（v5.1.3）— 2026-07-24
- UI 微调（cpp-pro 执行）：
  - 功能项名称 15→16px、说明文字 14→15px（卡片高度 108→116 联动，状态徽章下移防重叠）。
  - 页脚提示 13→14px、底部版权 12→13px（绘制矩形同步加高防裁切）。
- 版权信息 `© 重庆持玛多网络科技有限公司 · Z.W.` 已在界面底部绘制（与 exe 属性一致），本次仅随字号放大保持清晰。
- 构建：73/0 自检通过（沙箱非提权，HKLM 写入走 INFO 分支，非回归）。UI 为目测类改动，需真机确认。

## v1.0.0.18（v5.1.2）— 2026-07-23
- 内存安全体检（cpp-pro + 自审）：
  - `SubstituteSelectionPath` 弃用 `MAX_PATH` 固定缓冲，改用 `GetDisplayName` 返回的 CoTaskMem 堆串（防深目录 >260 字符截断）。
  - `MakeTerminalIcon` 修复 `hbmMask` 兜底 `CreateBitmap` 路径句柄泄漏。
  - `WriteIcoFromHICON` 防御 NULL 位图。
  - `RegWriteString` 防御 NULL value（wcslen 越界）。
  - 自检 `TestWin11ComServer` 补 `CoInitializeEx` / `CoUninitialize`。
- Windows 10+ / Server 兼容性审计：
  - 64 位产物匹配 64 位 Explorer，无 WOW64 问题。
  - API（`RegDeleteTreeW` / `SHGetKnownFolderPath` / `IExplorerCommand` / `GetModuleHandleExW`）均 ≥ Vista，覆盖 Server 2016/2019/2022。
  - 桌面背景目录回退改用 `FOLDERID_Desktop`。
- **真机实测**：Windows Server 2022 (Desktop Experience) 右键项正常显示与工作。

## v1.0.0.17（v5.1.1）
- 修复「显示/隐藏 停用误报」（注册表删除被拒绝的误判）。
- 修复「命令提示符未注册到背景」。

## v1.0.0.x（v5.1）
- 混合架构落地：管理器 exe + 独立 COM 服务器 DLL（无提权清单），DLL 内嵌 exe 并由 `EnsureShellDll()` 释放。
- 解决 %V 占位符在 Win11 新菜单失效（COM 服务器 Invoke 中 `SubstituteSelectionPath` 替换）。

## 更早
- v4.x：2 文件方案（exe + DLL 无提权清单），运行正常。
- v5.0：单文件 EXE-as-COM-server 尝试 → 失败（带提权清单的 exe 不能被 Explorer 加载为 InProcServer32）。
