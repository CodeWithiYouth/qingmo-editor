# 源码来源与版本

QingMo 不是从 GitHub 上 fork 出来的仓库，而是从 SciTE 官方源码包导入、重组并修改后的派生项目。

## 上游源码包

- 项目：SciTE 5.6.7
- 下载页：<https://www.scintilla.org/SciTEDownload.html>
- 源码包：<https://www.scintilla.org/scite567.zip>
- 下载时间：2026-10-26
- 文件大小：5,727,593 字节
- SHA-256：`017ADA7525C1FD2D50611FCDCA88572807BE730D7C548F31EDE0F35DD950D8F5`

`scite567.zip` 内自带以下组件，不需要另外下载：

| 组件 | 版本 | 包内目录 |
|---|---|---|
| SciTE | 5.6.7 | `scite/` |
| Scintilla | 5.6.7 | `scintilla/` |
| Lexilla | 5.5.4 | `lexilla/` |
| Lua | 5.3.4 | `scite/lua/` |

上游源码包的 `.hg_archival.txt` 记录：

```text
repo: dfc7523942ae501c035e4c36e791803c09d112e5
node: faccadbfdf959954854418a701be7a785b7227e2
branch: default
tag: rel-5-6-7
```

## 导入映射

| 上游路径 | 本项目路径 | 说明 |
|---|---|---|
| `scite/src/` | `src/` | SciTE 应用层核心 |
| `scite/win32/` | `src/win32/` | Windows 壳层、菜单、对话框、资源 |
| `scite/src/*.properties` | `config/` | 全局与语言配置 |
| `scite/win32/locale.properties` | `config/locale.properties` | 英文语言模板 |
| `scite/lua/` | `third_party/lua/` | Lua 5.3.4 |
| `scintilla/` | `third_party/scintilla/` | Scintilla 5.6.7 |
| `lexilla/` | `third_party/lexilla/` | Lexilla 5.5.4 |

导入时删除了上游的构建脚本、测试目录、GTK/Qt/Cocoa 平台代码和部分文档，保留了编译需要的源码、资源、许可证和 README。`src/`、`src/win32/` 里来自 SciTE 的文件保留原始版权头。

## 中文语言包

- 基线：SciTE 官方简体中文翻译 1.73
- 地址：<https://www.scintilla.org/locale.zh_gb.properties>
- 原始编码：GBK
- 本项目转换为 UTF-8，补充并修订到 SciTE 5.6.7，保存在 `config/locale.zh.properties`
- 译者署名见 `THIRD_PARTY_NOTICES.md` 和语言包文件头

## 为什么 GitHub 显示“不是 Fork”

GitHub 的 **Fork** 按钮只能 fork **GitHub 上已存在的仓库**，并会保留 `forked from` 关系。SciTE 官方仓库不在 GitHub 上，而是 SourceForge/Mercurial 的源码归档，因此没有可直接 fork 的官方上游。

本仓库是：

1. 下载官方 `scite567.zip`
2. 把 SciTE/Scintilla/Lexilla/Lua 源码重新组织到 `src/`、`third_party/`
3. 改名为 QingMo，加入图标、中文、JSON 格式化、标签关闭按钮等修改
4. 用新的 Git 历史提交到 GitHub

所以它是一个**独立的派生仓库**，GitHub 不会显示 `forked from`。上游归属通过 README、`THIRD_PARTY_NOTICES.md`、`LICENSES/` 和本文件说明。

## 本地归档

下载的原始包保留在：

```text
D:\gll\editor\_download\scite567.zip
```

`_download/` 已加入 `.gitignore`，不会提交到 GitHub。GitHub 上保存的是导入后的源码副本。
