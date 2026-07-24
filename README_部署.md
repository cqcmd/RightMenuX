# RightMenuX 部署说明

> 版本：**v1.0.2.20260724**（v5.1.5）
> 适用：Windows 10 / 11 / Windows Server 2016 / 2019 / 2022（64 位）

## 这是什么

RightMenuX 是一款 Windows 右键菜单增强工具，提供两项常用能力：

- **命令提示符**：在文件夹、桌面空白处右键，一键打开终端并 `pushd` 进入当前目录。
- **显示 / 隐藏 系统文件**：一键切换资源管理器对受保护系统文件（如 `ProgramData` 下的隐藏项）的显隐。

## 系统要求

| 项目 | 说明 |
|------|------|
| 操作系统 | Windows 10 / 11 / Server 2016 / 2019 / 2022（**64 位**） |
| 桌面环境 | 必须带 **桌面体验（Desktop Experience）** 角色。Server Core 无 Explorer，右键菜单不会出现 |
| 权限 | 必须**以管理员身份运行**（写 `HKLM` 注册表、释放 COM 服务器到 `C:\Windows`） |
| 外部依赖 | **无**。不依赖 MinGW / VC++ 运行库，依赖全是 Windows 自带系统 DLL |

## 部署步骤

1. 将单文件 `RightMenuX.exe` 复制到目标机器任意目录（如 `C:\Tools\`）。
2. **右键 → 以管理员身份运行**。
3. 在 GUI 中启用所需功能（命令提示符 / 显示隐藏系统文件）。
4. 右键菜单**即时生效**，无需重启。

## 单文件分发说明

`RightMenuX.exe` 为**自包含单文件**：

- COM 服务器 `RightMenuXShell.dll` 已以资源形式内嵌于 exe，首次启用功能时由 `EnsureShellDll()` 自动释放到 `C:\Windows\RightMenuXShell.dll`（失败则回退 `System32`）。
- 因此部署时**只需分发这一个 exe 文件**，无需携带独立 dll 或运行库。

## 卸载

1. 在 GUI 中停用全部功能。
2. 手动删除已释放的 `C:\Windows\RightMenuXShell.dll`（若存在）与 `RightMenuX.exe`。

## 兼容性备注

- 64 位产物与 64 位 Explorer 位数一致，不存在 32 位 / WOW64 镜像注册问题。
- 已实测：Windows Server 2022 (Desktop Experience) 右键项正常显示与工作。
- Windows 10 经典菜单与 Windows 11 新菜单均兼容（新菜单走 `DelegateExecute`，经典菜单走 `command` 子键回退）。
