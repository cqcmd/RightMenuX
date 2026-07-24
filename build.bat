@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

:: ============================================================
:: RightMenuX Build Script (MinGW-w64) —— v5.1 混合架构
:: 产物：
::   RightMenuX.exe       管理器 GUI（requireAdministrator）
::   RightMenuXShell.dll   进程内 COM 服务器（无提权清单），由 exe 内嵌，
::                          启用功能时释放到 C:\Windows 并注册
::
:: 设计要点：
::   - exe 带 requireAdministrator 清单，若同时当 COM 服务器会被 COM 拒绝在
::     Explorer 内加载（命令提示符不显示 / 显示隐藏无效的根因）。
::   - 故 COM 服务器单独编译成无提权清单的 RightMenuXShell.dll。
::   - explorercommand.cpp 通过 -DBUILD_SHELL_DLL 排除 _mcentry（exe 双形态入口）。
::
:: 注意：本脚本用 cd /d %~dp0 切到自身目录，避免调用方 CWD 含中文导致的路径乱码。
:: ============================================================
cd /d "%~dp0"

set "VERSION_FILE=version.ini"

:: 自动定位 MinGW
set "MINGW="
if exist "C:\mingw64\bin\g++.exe"      set "MINGW=C:\mingw64\bin"
if exist "C:\msys64\mingw64\bin\g++.exe" set "MINGW=C:\msys64\mingw64\bin"
if not "%MINGW%"=="" (
    set "PATH=%MINGW%;%PATH%"
) else (
    echo [提示] 未找到固定位置的 MinGW，将依赖 PATH 中的 g++。
)

where g++.exe >nul 2>&1
if errorlevel 1 (
    echo ERROR: 找不到 g++，请先安装 MinGW-w64 并将其加入 PATH。
    exit /b 1
)

:: 生成图标（若不存在）
if not exist "app.ico" (
    echo [0/5] 生成图标...
    python gen_icon.py
)

:: 递增版本号并写入 app.rc 与 shell.rc
echo [1/5] 版本戳...
python version_stamp.py
if errorlevel 1 ( echo ERROR: 版本戳失败 & exit /b 1 )

:: ---- 编译 Shell COM 服务器 DLL（无提权清单）----
echo [2/5] 编译 RightMenuXShell.dll...
windres -i shell.rc -o shellres.o
if errorlevel 1 ( echo ERROR: shell.rc 编译失败 & exit /b 1 )
g++ -O2 -Wall -ffunction-sections -fdata-sections -DBUILD_SHELL_DLL -shared ^
    -finput-charset=UTF-8 -DUNICODE -D_UNICODE ^
    explorercommand.cpp shellres.o ^
    -o RightMenuXShell.dll ^
    -lshell32 -lshlwapi -ladvapi32 -luser32 -lole32 -luuid ^
    -static -Wl,--kill-at -Wl,--dynamicbase,--nxcompat -s
if errorlevel 1 ( echo ERROR: RightMenuXShell.dll 编译失败 & exit /b 1 )
del /q shellres.o 2>nul

:: ---- 编译管理器 exe 的资源（内嵌 RightMenuXShell.dll 为 RCDATA 200）----
:: 注意：必须在 DLL 生成之后，windres 才能找到 RightMenuXShell.dll 文件。
echo [3/5] 编译资源 res.o（含内嵌 DLL）...
windres -i app.rc -o res.o
if errorlevel 1 ( echo ERROR: 资源编译失败 & exit /b 1 )

:: ---- 编译管理器 exe（main + features + selftest，不含 COM 服务器源码）----
echo [4/5] 编译 RightMenuX.exe...
g++ -O2 -Wall -ffunction-sections -fdata-sections -mwindows -municode ^
    -DUNICODE -D_UNICODE -finput-charset=UTF-8 ^
    main.cpp features.cpp selftest.cpp res.o ^
    -o build_main.exe ^
    -lshell32 -lshlwapi -ladvapi32 -luser32 -lole32 -luuid ^
    -static -Wl,--kill-at -Wl,--dynamicbase,--nxcompat -Wl,--gc-sections -s
if errorlevel 1 ( echo ERROR: 编译失败 & exit /b 1 )

:: 清理
del /q res.o 2>nul

:: 宽字符重命名 -> 正确文件名（与代码页无关，避免乱码）
echo [5/5] 重命名为最终文件名...
python rename_outputs.py
if errorlevel 1 ( echo ERROR: 重命名失败 & exit /b 1 )

echo.
echo ========================================
echo 构建成功！
echo   RightMenuX.exe        - 管理器 GUI（requireAdministrator）
echo   RightMenuXShell.dll   - COM 服务器（无提权清单，内嵌于 exe）
echo   启用功能时自动释放到 C:\Windows 并注册。
echo   自检：RightMenuX.exe --selftest
echo ========================================
endlocal
