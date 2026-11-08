#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

// 前向声明，不需要完整类型
class HttpServer;



extern int g_receive_type;
extern int g_StartPort;
extern int g_MsgSendPort;
extern HMODULE g_hWeixinDll;
extern HMODULE g_hWeixinExe;


extern HANDLE g_hLoginMonitor;
extern HANDLE g_hAfterLoginInit;

extern DWORD g_MainThreadId;
extern DWORD 进程PID;
extern DWORD 父进程PID;
extern HWND  g_WeixinMainHwnd;
extern volatile uint64_t g_IsLogin;   // 0=未登录 1=已登录
extern volatile uint64_t g_getprofile;

extern volatile bool g_LoginMonitorRunning;
extern std::string g_CallBack_Url;
extern CRITICAL_SECTION g_dbMgrCriticalSection;

extern std::wstring g_AppDataDir;
extern std::wstring g_DocumentDir;
extern std::wstring g_UsersDir;
extern HMODULE g_hModule;

extern uint64_t g_MyModuleBase;
extern uint64_t g_MyModuleSize;
extern uint64_t g_MyModuleEnd;


struct SelfInfo_t
{
    std::string wxid;
    std::string alias;
    std::string nickname;
    std::string phone;
    std::string email;

    uint64_t qq;
    std::string proiv;
    std::string area;
    std::string signinfo;


};
// 全局唯一实例
extern SelfInfo_t SelfInfo;

#define WX_ADDR(offset) ((void*)((uintptr_t)g_hWeixinDll + (offset)))
#define XWECHAT_MAIN_CLAZZ_OFFSET            ((void*)((uintptr_t)g_hWeixinDll + 0xA83AB20))	// 4.1.15.56 未适配（BSS 数据，Patch_Low_Version 已停用）

inline std::wstring g_MyDir;

inline HttpServer* g_httpServer = nullptr;

inline constexpr uint64_t g_Patch_Revoke = 0x230BBC7;	// 4.1.15.56 防撤回 patch 点（nop + jmp 覆盖 je）


constexpr size_t XWECHAT_SQLITE3_VFS_OFFSET = 0xB9DA500;	// 4.1.15.56
constexpr size_t XWECHAT_SQLITE3_API_ROUTINES_OFFSET = 0x9A907F0;	// 4.1.15.56
constexpr size_t XWECHAT_SQLCIPHER_API_ROUTINES_OFFSET = 0x9A91030;	// 4.1.15.56
constexpr size_t XWECHAT_SQLITE3_CODEC_GET_KEY_FUNC = 0x4EE64D0;	// 4.1.15.56 未适配（db_mgr 中为死代码，未验证）

// 4.1.15.56 核心偏移（由 4.1.10.27 锚定匹配得出）
namespace offset
{
    inline constexpr uint64_t dec_pic_call = 0x549CC0;	// 图片 dat 解密
    inline constexpr uint64_t create_param2 = 0xF910;	// 构造发送参数2
    inline constexpr uint64_t send_message = 0x19E5F00;	// 消息发送入口
    inline constexpr uint64_t param1_vtable = 0x92EA088;	// param1 分发表

    inline constexpr uint64_t param2 = 0xB322830;	// 全局参数指针
    // param2_2 / param2_3 二者代码完全一致，静态无法区分先后；
    // 按 .rdata 相邻顺序赋值，若发送失败可将两者互换验证。
    inline constexpr uint64_t param2_1 = 0x90186F8;
    inline constexpr uint64_t param2_2 = 0x934E298;
    inline constexpr uint64_t param2_3 = 0x934E1F8;
    inline constexpr uintptr_t txt_message_ctr = 0x77AB70;	// 文本消息构造器
    inline constexpr uintptr_t txt_message_vtbl = 0x9026AB8;
    inline constexpr uint64_t img_msg_vtbl = 0x92B2C78;
    inline constexpr uint64_t img_msg_vtb2 = 0x92B2D08;
}


//4.1.15.56  xml（数据块按内容匹配定位；FORWARD_XML_CALL 在新版本中已不存在）
namespace Offsets
{
    inline constexpr uintptr_t IMAGE_FIELD_VTABLE = 0x88ECF0C;
    inline constexpr uintptr_t IMAGE_FIELD_VTABLE2 = 0x88ECFBF;
    inline constexpr uintptr_t IMAGE_DATA_VTABLE = 0x8295D28;
    inline constexpr uintptr_t IMAGE_DATA_VTABLE2 = 0x8295DB8;
    inline constexpr uintptr_t VIDEO_FIELD_VTABLE = 0x838C7F8;
    inline constexpr uintptr_t VIDEO_FIELD_VTABLE2 = 0x838C888;
    inline constexpr uintptr_t ANIMATION_FIELD_VTABLE = 0x838CC18;
    inline constexpr uintptr_t ANIMATION_FIELD_VTABLE2 = 0x838CCA8;
    inline constexpr uintptr_t MESSAGE_STRUCT_VTABLE = 0x853D388;
    inline constexpr uintptr_t MESSAGE_STRUCT_VTABLE2 = 0x853D338;
    inline constexpr uintptr_t MESSAGE_PARAM_VTABLE = 0x853CF38;
    inline constexpr uintptr_t FORWARD_XML_CALL = 0x0;	// 4.1.15.56 已移除（旧 0x1CF3D20 为函数中部标签，新版本对应字段处理已删除），ForwardXMLMsg 将返回失败
}



