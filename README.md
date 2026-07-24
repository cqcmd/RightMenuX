# RightMenuX

Windows 右键菜单增强工具，纯 C++ / Win32 GDI 自绘，单 exe 双击即跑，无运行时依赖。

当前版本：**v1.0.2.20260724**（v5.1.5）

## 功能

| 功能 | 目标位置 | 说明 |
|------|----------|------|
| 计算机管理 | 此电脑右键 | 打开 `compmgmt.msc`（计算机管理） |
| 设备管理器 | 此电脑右键 | 打开 `devmgmt.msc`（设备管理器） |
| 命令提示符 | 文件夹 / 桌面空白 | 打开终端并 `pushd` 进入当前目录 |
| 显示 / 隐藏 系统文件 | 文件夹背景 | 一键切换资源管理器对受保护系统文件的显隐（SuperHidden 模块） |

> Win11 新菜单直显：计算机管理 / 设备管理器 / 命令提示符 通过通用 `IExplorerCommand` COM 服务器直绘入 Win11 新右键菜单，无需「显示更多选项」；SuperHidden 模块经文件夹背景右键生效。重启资源管理器后全部生效。

## 架构

混合架构（v5.1）：

- **`RightMenuX.exe`**：管理器 GUI，带 `requireAdministrator` 清单（写 `HKLM` 弹 UAC）。
- **`RightMenuXShell.dll`**：进程内 COM 服务器（`IExplorerCommand` + SuperHidden 切换），**无提权清单**（asInvoker）。以资源 RCDATA(200) **内嵌进 exe**，启用功能时由 `EnsureShellDll()` 释放到 `C:\Windows\RightMenuXShell.dll`（失败回退 `System32`）。
- Explorer 以中完整性级别运行，无法加载带提权清单的 InProcServer32，故 COM 服务器必须独立且无提权清单。

> ⚠️ 关键根因：带 `requireAdministrator` 的 exe 不能作为 InProcServer32 被 Explorer 加载（v5.0 单文件方案因此失败），故 v5.1 拆回独立 DLL。

## 兼容性

- 系统：Windows 10 / 11 / Server 2016 / 2019 / 2022（64 位）。
- 64 位产物与 64 位 Explorer 位数一致，无 WOW64 镜像注册问题。
- 必须带「桌面体验」角色（Server Core 无 Explorer）。
- 实测：Windows Server 2022 (Desktop Experience) 右键项正常显示与工作。

## 构建

需要 MinGW-w64（64 位）与 Python 3。在 Git Bash 中：

```bash
cd RightMenuX
export PATH="/c/msys64/mingw64/bin:$PATH"
PY="python.exe"   # 或你的 python 路径
"$PY" version_stamp.py && \
windres -i shell.rc -o shellres.o && \
g++ -O2 -Wall -DBUILD_SHELL_DLL -shared -finput-charset=UTF-8 -DUNICODE -D_UNICODE \
    explorercommand.cpp shellres.o -o RightMenuXShell.dll -lshell32 -lshlwapi -ladvapi32 -luser32 -lole32 -luuid -static -Wl,--kill-at -Wl,--dynamicbase,--nxcompat -s && \
windres -i app.rc -o res.o && \
g++ -O2 -Wall -mwindows -municode -DUNICODE -D_UNICODE -finput-charset=UTF-8 \
    main.cpp features.cpp selftest.cpp res.o -o build_main.exe -lshell32 -lshlwapi -ladvapi32 -luser32 -lole32 -luuid -static -Wl,--kill-at -Wl,--dynamicbase,--nxcompat -Wl,--gc-sections -s && \
"$PY" rename_outputs.py && \
./RightMenuX.exe --selftest-nogui
```

构建脚本 `build.bat` 等价于上述流程（版本号采用四段式 `主.次.修订.编译日期戳`，每次编译自动递增修订号并写入当天日期戳，详见 CHANGELOG）。

## 自检

`RightMenuX.exe --selftest-nogui` 无头运行，报告写入 `selftest_report.txt`，验证 COM 服务器导出、图标结构、注册表写回等。

## 目录说明

```
main.cpp            GUI 与主窗口绘制
features.cpp        图标生成、COM 服务器释放、注册表操作
explorercommand.cpp COM 服务器（IExplorerCommand / ToggleSuperHidden），仅编进 DLL
selftest.cpp        无头自检
common.h            公共工具（注册表读写等）
*.rc / resource.h   资源与版本
version_stamp.py    版本号自增
rename_outputs.py   重命名中间产物为 RightMenuX.exe
build.bat           一键构建
```
