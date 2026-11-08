# WeChat-Hook

Windows 微信（4.x）Hook 工具。通过加载器将 `version.dll` 注入微信主进程，在进程内启动本地 HTTP 服务，把微信的消息发送、图片解码、账号资料、SQLite 数据库查询能力暴露为一组 HTTP 接口，便于对接机器人、自动化、消息同步等业务系统。

- **适配版本**：微信 `4.1.15.56`（x64）
- **对接文档**：[对接文档.md](./对接文档.md)（面向接入开发者，含全部接口规格与多语言示例）
- **Postman 集合**：`postman/WeChat-Hook.postman_collection.json`

## 功能

| 能力 | 说明 | 接口 |
| --- | --- | --- |
| 发送文本 | 个人 / 群聊 | `POST /SendTextMsg` |
| 发送图片/文件 | 本机绝对路径，支持 jpg/png/gif/mp4/zip 等 | `POST /SendImgMsg` |
| 解密图片缓存 | 微信 `.dat` 加密图片还原 | `POST /Decode_Pic` |
| 账号资料 | 当前登录账号信息 | `POST /GetSelfProfile` |
| 数据库查询 | 进程内直连 SQLite（联系人/群/消息库） | `POST /QueryDB/execute`、`/QueryDB/GetAllDBName` |
| 登录状态 | 供对接方轮询判断就绪 | `GET /QueryDB/status` |

> XML 卡片消息转发（`/ForwardXMLMsg`）在 4.1.15.56 下不可用，详见[对接文档](./对接文档.md)。

## 快速开始

```bat
:: 1. 编译（Visual Studio 2022 / MSBuild，x64 Release）
MSBuild.exe x64_Version_dll.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64
cl /nologo /O2 /EHsc /D_UNICODE /DUNICODE wxinject.cpp /Fe:wxinject.exe

:: 2. 部署：version.dll 放到微信安装目录
copy x64\Release\version.dll "D:\Program Files\Tencent\Weixin\"

:: 3. 屏蔽自动升级（升级会替换目录并使偏移失效）
防止微信自动更新\4.x禁止微信自动升级.bat

:: 4. 通过加载器启动微信（不要直接双击微信）
wxinject.exe "D:\Program Files\Tencent\Weixin\Weixin.exe"

:: 5. 验证
curl http://127.0.0.1:30001/QueryDB/status
```

登录微信后即可调用接口。完整对接说明见[对接文档](./对接文档.md)。

## 工作原理

```
wxinject.exe ──CreateProcess(SUSPENDED)──► Weixin.exe
     │              等待 Weixin.dll 加载
     └────CreateRemoteThread(LoadLibraryW)──► version.dll 注入主进程
                                                │
                                     代理系统 version.dll 导出
                                     应用防撤回 patch
                                     启动 HTTP 服务 :30001
```

微信 4.1.15+ 的 `Weixin.exe` 不再从安装目录解析 `version.dll`（`version.dll` 属于 KnownDLLs，固定映射 system32），因此必须以注入方式加载，本项目自带加载器 `wxinject.cpp`。

## 项目结构

```
├── wxinject.cpp            加载器源码（挂起启动 → 等 Weixin.dll → 注入 → 放行）
├── dllmain.cpp             DLL 入口：启动参数解析、真实 version.dll 加载、主进程判定
├── include/
│   ├── global.h            全部偏移定义（按微信版本维护，升级时重点修改）
│   ├── http_server.h 等    HTTP 服务、路由、JSON、工具头文件
├── src/
│   ├── version_proxy.cpp   代理系统 version.dll 的 17 个导出函数
│   ├── inline_weixin_dll_load.cpp  Weixin.dll 就绪初始化、防撤回 patch、服务启动
│   ├── wx_send.cpp         消息结构体构建与发送（文本/图片）
│   ├── wx_send_xml.cpp     XML 卡片消息构建（4.1.15.56 下接口停用）
│   ├── SendTextMsg.cpp / SendImageMsg.cpp / ForwardXMLMsg.cpp / GetSelfProfile.cpp
│   ├── QueryDB.cpp         数据库查询接口
│   ├── http_server.cpp / http_routes.cpp / requests.cpp
│   └── Hook_Method.cpp     call 指令级 hook 框架（trampoline + near jump）
├── xdb/                    SQLite api_routines 直连封装（db_mgr、pattern 扫描）
├── 3rdparty/               md5、httplib
├── minhook/                MinHook（事务性 inline hook）
├── postman/                Postman 接口集合
└── 防止微信自动更新/        屏蔽微信自动升级的脚本
```

## 适配新版本

本项目依赖微信内部偏移与结构体布局，微信升级后需要重新定位。本项目已按 4.1.15.56 完成适配，主要偏移集中在 `include/global.h`：

| 类别 | 内容 |
| --- | --- |
| 核心函数 | `send_message` / `create_param2` / `dec_pic_call` / `txt_message_ctr` |
| 分发表 | `param1_vtable` / `param2` / `param2_1~3` / `txt_message_vtbl` / `img_msg_vtbl/vtb2` |
| 防撤回 | `g_Patch_Revoke`（`nop + jmp` 覆盖条件跳转） |
| SQLite | `XWECHAT_SQLITE3_API_ROUTINES_OFFSET` / `SQLCIPHER` / `VFS` |

适配要点：

1. 结构体布局随版本变化：4.1.15.56 相对 4.1.10.27，消息结构 `0xD8` 起整体 `+0x40`，`0x6A8` 起再 `+0x50`（详见 `src/wx_send.cpp` 内注释与 `static_assert`）。
2. 定位方法：取旧版安装包（[历史版本归档](https://github.com/cscnk52/wechat-windows-versions)）做参照，通过微信自身的消息工厂函数做跨版本指令级对齐，比裸特征码更稳。
3. 历史版本偏移见 git 历史（`include/global.h` 的上一版）。

## 常见问题

- **30001 端口不监听**：微信未通过加载器启动，或版本不是 4.1.15.56（微信会自动升级，注意检查模块路径）。
- **接口连接失败**：本机代理拦截了 127.0.0.1，请求需绕过代理（`--noproxy "*"` / `trust_env=False`）。
- **发送返回 success 但未送达**：接口返回仅代表投递成功，送达以微信 UI / 数据库为准；高频发送会触发风控，请自行限流。
- **GetAllDBName 返回空**：未登录，登录后重试。

更多排查项见[对接文档](./对接文档.md)第 9 节。

## 声明

本项目仅供学习研究与自动化对接使用，请勿用于违法违规用途，使用产生的后果由使用者自行承担。
