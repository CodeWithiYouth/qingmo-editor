# 轻墨 QingMo

基于 **SciTE 5.6.7** 的轻量 Windows 文本编辑器骨架。

目标是把 SciTE 的“小、快、稳定”保留下来，逐步替换界面和交互，做成一个现代化的轻量编辑器，而不是从零重写文本内核。

## 当前状态

- 单个静态链接的 `QingMo.exe`，约 **3.6 MB**
- 已内置 Scintilla 5.6.7、Lexilla 5.5.4、Lua 5.3.4
- SciTE 原有能力全部保留：
  - 多标签、分屏、项目/目录打开
  - 语法高亮（Lexilla 全量 lexer）
  - 查找/替换、正则、在文件中查找
  - 编码识别与转换（UTF-8 / UTF-16 / 代码页）
  - 换行符 LF / CRLF / CR
  - Lua 脚本扩展
  - 会话、书签、折叠、宏、导出 HTML/RTF/PDF 等
- 窗口标题已改为 `轻墨`
- 默认界面语言为简体中文（语言包 `config/locale.zh.properties`）
- 新图标：墨滴 + 文本光标（`src/win32/QingMo.ico`，16–256 多尺寸）
- **尚未做**：关于页正文、安装包品牌化，UI 现代化，配置精简

## 目录结构

```
CMakeLists.txt              # 唯一的构建入口
src/                        # SciTE 应用层（core，平台无关）
src/win32/                  # Windows 壳层、资源、菜单、对话框
config/                     # .properties 配置、中文语言包、Lua 内嵌脚本
third_party/
  scintilla/                # 编辑内核 5.6.7
  lexilla/                  # 语法分析器 5.5.4
  lua/                      # Lua 5.3.4
scripts/
  build.ps1                 # 一键配置 + 编译
  run.ps1                   # 运行已编译的程序
  make_icon.py              # 重新生成应用图标（Pillow）
build/                      # 构建产物（已忽略）
_download/                  # 上游原始压缩包/源码（已忽略，可删除）
```

`src/` 和 `src/win32/` 来自 `scite/src`、`scite/win32`，保留了上游文件结构和版权头。当前对上游的改动：

1. `src/win32/SciTEWin.cxx`：`appName` 改为 `轻墨`
2. `src/win32/SciTEWinDlg.cxx`：关于框标题改为 `轻墨`
3. `src/win32/SciTERes.rc`：`Embedded.properties` 指向 `config/`，关于框与版本资源改为 `轻墨`
4. `src/win32/QingMo.exe.manifest`：程序集名称改为 `QingMo`
5. `config/locale.zh.properties`：新增简体中文语言包
6. `config/SciTEGlobal.properties`：默认加载中文语言包

## 构建

**要求**：Windows 10/11、Visual Studio 2022 或 2026（含 C++ 桌面开发）、CMake 3.25+。

使用 VS 2026 自带 CMake：

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release --parallel
```

或直接运行脚本（会自动定位 Visual Studio）：

```powershell
.\scripts\build.ps1
```

产物：

```
build\bin\QingMo.exe
build\bin\*.properties
```

`config/` 会在每次构建后自动复制到 exe 同目录，保持绿色便携。

运行：

```powershell
.\scripts\run.ps1
# 或
.\build\bin\QingMo.exe
```

打包成一个可分发的文件夹：

```powershell
cmake --build build --target package --config Release
# 输出：build\package\QingMo.exe + 配置
```

## 中文界面

默认使用简体中文，语言包位于 `config/locale.zh.properties`。

- 基于 SciTE 官方简体中文翻译（1.43–1.74），已补充并修订到 5.6.7
- 文件编码 UTF-8（无 BOM），通过 `translation.encoding=UTF-8` 声明
- 菜单、对话框、状态消息统一走 SciTE 的 `localiser`，不需要改动 C++
- 切换回英文：把 `config/SciTEGlobal.properties` 中的
  `locale.properties=locale.zh.properties` 注释掉，重新构建或手动复制配置到 exe 目录
- 增加其他语言：复制 `locale.zh.properties` 为 `locale.xx.properties`，翻译右侧的值，
  再把 `locale.properties` 指向新文件

## 图标

- 设计：深色圆角底 + 白色墨滴 + 青色文本光标
- 源脚本：`scripts/make_icon.py`（Pillow）
- 生成物：`src/win32/QingMo.ico`、`src/win32/QingMoBusy.ico`、`resources/QingMo-512.png`
- 重新生成：`python scripts/make_icon.py`

## 关键设计

- **单 exe、无运行时 DLL 依赖**：Scintilla、Lexilla、Lua 全部静态链接；只需要 VC 运行库（大多数 Windows 已安装）。
- **配置与代码分离**：`config/` 是唯一的配置来源，构建时复制到 exe 旁边；用户改动不会污染源码。
- **保留 SciTE 的 properties 体系**：`SciTEGlobal.properties` 是总入口，语言配置按需 `import`，新增语言不需要改 C++。
- **Lua 作为扩展层**：后续可以把“命令面板、插件、宏”优先做成 Lua 扩展，减少 C++ 改动。
- **编辑内核不碰**：文本缓冲、编码、IME、绘制、滚动都由 Scintilla 负责，这是“轻量且稳定”的基础。

## 下一步建议

1. **品牌化收尾**：关于页正文、版本号、窗口类名、安装包图标。
2. **配置精简**：只保留常用语言和高亮，减少首包体积和启动扫描。
3. **现代 UI**：工具栏/标签页/状态栏重做，暗色主题默认化。
4. **命令面板**：`Ctrl+Shift+P`，作为新功能的统一入口。
5. **大文件模式**：只读 + 分块加载 + 关闭高亮，补齐 SciTE 的短板。
6. **设置界面**：把 properties 包装成可视化设置，不再要求用户手改配置文件。

## 上游与许可证

本项目包含以下上游代码，版权归原作者所有：

| 组件 | 版本 | 许可证 |
|---|---|---|
| SciTE | 5.6.7 | HPND 风格（见 `third_party/scintilla/License.txt`） |
| Scintilla | 5.6.7 | HPND 风格 |
| Lexilla | 5.5.4 | HPND 风格 |
| Lua | 5.3.4 | MIT |

完整文本见 `LICENSES/` 与各 `third_party/*/License.txt`。

> `_download/` 是导入时留下的上游原始包，不参与构建，可以安全删除。
