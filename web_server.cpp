#include "web_server.h"
#include "save_editor.h"
#include "trainer_client.h"
#include "config.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
std::atomic<bool> g_running(false);
std::mutex g_apiLock;
SOCKET g_listen = INVALID_SOCKET;
std::thread g_acceptThread;
int g_port = 0;
std::string g_root;
std::string g_customBg;

std::string ExeDir()
{
    char buf[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n == 0) return ".";
    std::string p(buf, n);
    size_t pos = p.find_last_of("\\/");
    return pos == std::string::npos ? std::string(".") : p.substr(0, pos);
}

std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); i++)
    {
        unsigned char c = (unsigned char)s[i];
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20)
                {
                    char esc[8];
                    _snprintf(esc, sizeof(esc), "\\u%04x", c);
                    out += esc;
                }
                else
                {
                    out += (char)c;
                }
        }
    }
    return out;
}

bool ReadFileBytes(const std::string& path, std::string& out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return false; }
    out.resize((size_t)len);
    size_t got = len > 0 ? fread(&out[0], 1, (size_t)len, f) : 0;
    fclose(f);
    if (got != (size_t)len) { out.clear(); return false; }
    return true;
}

bool WriteFileBytes(const std::string& path, const std::string& data)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t n = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return n == data.size();
}

bool Base64Decode(const std::string& src, std::string& dst)
{
    static const char* kTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int rev[256];
    for (int i = 0; i < 256; i++) rev[i] = -1;
    for (int i = 0; i < 64; i++) rev[(unsigned char)kTable[i]] = i;
    dst.clear();
    dst.reserve(src.size() / 4 * 3 + 3);
    int val = 0;
    int bits = 0;
    for (size_t i = 0; i < src.size(); i++)
    {
        unsigned char ch = (unsigned char)src[i];
        if (ch == '=') break;
        int d = rev[ch];
        if (d < 0) continue;
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            dst += (char)((val >> bits) & 0xFF);
        }
    }
    return !dst.empty();
}

int GetIntField(const std::string& body, const std::string& key, int def)
{
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return def;
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return def;
    const char* s = body.c_str() + p + 1;
    char* end = NULL;
    long value = strtol(s, &end, 10);
    if (end == s) return def;
    return (int)value;
}

long long GetInt64Field(const std::string& body, const std::string& key, long long def)
{
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return def;
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return def;
    const char* s = body.c_str() + p + 1;
    char* end = NULL;
    long long value = strtoll(s, &end, 10);
    if (end == s) return def;
    return value;
}

double GetFloatField(const std::string& body, const std::string& key, double def)
{
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return def;
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return def;
    const char* s = body.c_str() + p + 1;
    char* end = NULL;
    double value = strtod(s, &end);
    if (end == s || !std::isfinite(value)) return def;
    return value;
}

bool GetBoolField(const std::string& body, const std::string& key, bool def)
{
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return def;
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return def;
    const char* s = body.c_str() + p + 1;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (_strnicmp(s, "true", 4) == 0) return true;
    if (_strnicmp(s, "false", 5) == 0) return false;
    return atoi(s) != 0;
}

void GetIntArrayField(const std::string& body, const std::string& key, std::vector<int>& out)
{
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return;
    p = body.find('[', p + needle.size());
    if (p == std::string::npos) return;
    size_t end = body.find(']', p);
    if (end == std::string::npos) return;

    const char* s = body.c_str() + p + 1;
    const char* limit = body.c_str() + end;
    while (s < limit) {
        while (s < limit && (*s == ' ' || *s == ',' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
        if (s >= limit) break;
        char* next = NULL;
        long value = strtol(s, &next, 10);
        if (next == s) break;
        out.push_back((int)value);
        s = next;
    }
}

std::string GetStringField(const std::string& body, const std::string& key)
{
    std::string needle = "\"" + key + "\"";
    size_t p = body.find(needle);
    if (p == std::string::npos) return "";
    p = body.find(':', p + needle.size());
    if (p == std::string::npos) return "";
    p = body.find('"', p);
    if (p == std::string::npos) return "";
    size_t end = body.find('"', p + 1);
    if (end == std::string::npos) return "";
    return body.substr(p + 1, end - p - 1);
}

int QueryInt(const std::string& path, const std::string& key, int def)
{
    size_t q = path.find('?');
    if (q == std::string::npos) return def;
    std::string needle = key + "=";
    size_t p = path.find(needle, q);
    if (p == std::string::npos) return def;
    const char* s = path.c_str() + p + needle.size();
    char* end = NULL;
    long value = strtol(s, &end, 10);
    if (end == s) return def;
    return (int)value;
}

std::string ErrorText(int code)
{
    switch (code)
    {
        case 0: return "完成";
        case 1: return "完成（已为该存档激活对应 DLC）";
        case -1: return "存档文件未找到";
        case -2: return "内存不足";
        case -3: return "无法解析存档";
        case -4: return "写入失败";
        case -5: return "该存档没有激活对应 DLC";
        case -6: return "该 Boss 无方案A事件（本体终战无法用存档触发）";
        case -7: return "存档结构不支持该操作";
        case -8: return "该存档里没有好感数据";
        case -9: return "请求数据不合法";
        default: return "错误代码: " + std::to_string(code);
    }
}

std::string Result(int code, const std::string& msg = "")
{
    std::string text = msg.empty() ? ErrorText(code) : msg;
    std::string body = "{\"ok\":";
    body += code >= 0 ? "true" : "false";
    body += ",\"code\":" + std::to_string(code);
    body += ",\"msg\":\"" + JsonEscape(text) + "\"}";
    return body;
}

std::string SaveFolder()
{
    char folder[MAX_PATH] = {0};
    if (SaveEditor_GetSaveFolder(folder, sizeof(folder))) return "";
    return folder;
}

bool SlotPath(int slot, std::string& path)
{
    char buf[MAX_PATH] = {0};
    if (SaveEditor_GetPath(slot, buf, sizeof(buf))) return false;
    path = buf;
    return true;
}

int ReadSlotFund(int slot)
{
    std::string path;
    if (!SlotPath(slot, path)) return -1;
    std::string buf;
    if (!ReadFileBytes(path, buf)) return -1;
    size_t p = buf.find("\"fund\"");
    if (p == std::string::npos) return -1;
    p = buf.find(':', p);
    if (p == std::string::npos) return -1;
    return atoi(buf.c_str() + p + 1);
}

struct SaveInfo
{
    int slot;
    long long size;
};

std::string SavesJson()
{
    std::string folder = SaveFolder();
    std::vector<SaveInfo> found;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((folder + "\\Mystia#*.memory").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            int slot = 0;
            if (sscanf(fd.cFileName, "Mystia#%d.memory", &slot) == 1)
            {
                SaveInfo info;
                info.slot = slot;
                info.size = ((long long)fd.nFileSizeHigh << 32) | (long long)fd.nFileSizeLow;
                found.push_back(info);
            }
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    std::sort(found.begin(), found.end(), [](const SaveInfo& a, const SaveInfo& b) { return a.slot < b.slot; });

    std::string body = "{\"ok\":true,\"folder\":\"" + JsonEscape(folder) + "\",\"saves\":[";
    for (size_t i = 0; i < found.size(); i++)
    {
        if (i) body += ",";
        body += "{\"slot\":" + std::to_string(found[i].slot) + ",\"size\":" + std::to_string(found[i].size) + "}";
    }
    body += "]}";
    return body;
}

std::string BossesJson()
{
    std::string body = "[";
    int count = SaveEditor_GetBossCount();
    for (int i = 0; i < count; i++)
    {
        if (i) body += ",";
        body += "{\"id\":" + std::to_string(i);
        body += ",\"label\":\"" + JsonEscape(SaveEditor_GetBossLabel(i)) + "\"";
        body += ",\"methods\":\"" + JsonEscape(SaveEditor_GetBossMethods(i)) + "\"";
        body += ",\"desc\":\"" + JsonEscape(SaveEditor_GetBossDesc(i)) + "\"";
        body += ",\"hasQueue\":" + std::string(SaveEditor_BossHasQueue(i) ? "true" : "false");
        body += ",\"hasClear\":" + std::string(SaveEditor_BossHasClear(i) ? "true" : "false");
        body += ",\"hasInvite\":" + std::string(SaveEditor_BossHasInvite(i) ? "true" : "false");
        body += "}";
    }
    body += "]";
    return body;
}

std::string MetaJson()
{
    std::string body = "{\"version\":\"" VERSION "\"";
    body += ",\"supported\":\"" + JsonEscape(SUPPORTED_VERSION) + "\"";
    body += ",\"folder\":\"" + JsonEscape(SaveFolder()) + "\"";
    body += ",\"mapCount\":" + std::to_string(SaveEditor_GetMapCount());
    body += ",\"customBg\":" + std::string(GetFileAttributesA(g_customBg.c_str()) == INVALID_FILE_ATTRIBUTES ? "false" : "true");
    body += "}";
    return body;
}

std::string CommandError(const std::string& reply)
{
    if (reply.compare(0, 3, "ERR") == 0) return reply.size() > 4 ? reply.substr(4) : reply;
    return reply;
}

std::string TrainerStatusJson()
{
    TrainerStatus st;
    bool ok = TrainerClient_Status(&st);
    std::string body = "{\"ok\":";
    body += ok ? "true" : "false";
    body += ",\"game\":";
    body += st.gameRunning ? "true" : "false";
    body += ",\"attached\":";
    body += st.attached ? "true" : "false";
    body += ",\"resolved\":";
    body += st.resolved ? "true" : "false";
    body += ",\"moneyReady\":";
    body += st.moneyReady ? "true" : "false";
    body += ",\"speedReady\":";
    body += st.speedReady ? "true" : "false";
    body += ",\"frozen\":";
    body += st.frozen ? "true" : "false";
    body += ",\"money\":" + std::to_string(st.money);
    char speed[64] = {0};
    _snprintf(speed, sizeof(speed) - 1, "%.4f", st.timeScale);
    body += ",\"timeScale\":" + std::string(speed);
    body += ",\"error\":\"" + JsonEscape(st.error) + "\"}";
    return body;
}

bool IsElevated()
{
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation;
    DWORD size = sizeof(elevation);
    bool elevated = GetTokenInformation(token, TokenElevation, &elevation, size, &size) != FALSE &&
                    elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return elevated;
}

std::string SelfCheckJson()
{
    std::vector<std::string> lines;
    std::string exeDir = ExeDir();
    lines.push_back("程序目录 " + exeDir);
    lines.push_back(std::string("核心 DLL ") +
        (GetFileAttributesA((exeDir + "\\IzakayaCheatCore.dll").c_str()) != INVALID_FILE_ATTRIBUTES ? "已找到" : "缺失"));
    lines.push_back(std::string("WebView2Loader ") +
        (GetFileAttributesA((exeDir + "\\WebView2Loader.dll").c_str()) != INVALID_FILE_ATTRIBUTES ? "已找到" : "缺失"));
    lines.push_back(std::string("web 资源 ") +
        (GetFileAttributesA((g_root + "\\index.html").c_str()) != INVALID_FILE_ATTRIBUTES ? "已找到" : "缺失"));
    lines.push_back(std::string("web app.js ") +
        (GetFileAttributesA((g_root + "\\app.js").c_str()) != INVALID_FILE_ATTRIBUTES ? "已找到" : "缺失"));
    lines.push_back(std::string("web style.css ") +
        (GetFileAttributesA((g_root + "\\style.css").c_str()) != INVALID_FILE_ATTRIBUTES ? "已找到" : "缺失"));
    lines.push_back(std::string("角色映射 ") +
        (GetFileAttributesA((g_root + "\\character_names.json").c_str()) != INVALID_FILE_ATTRIBUTES ? "已找到" : "缺失"));
    char module[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, module, MAX_PATH);
    lines.push_back(std::string("主程序 ") + (module[0] ? module : "未知"));
    lines.push_back(std::string("版本 ") + VERSION + " / 存档 " + SUPPORTED_VERSION);
    lines.push_back("地图数量 " + std::to_string(SaveEditor_GetMapCount()));
    lines.push_back("Boss 数量 " + std::to_string(SaveEditor_GetBossCount()));
    lines.push_back(std::string("管理员权限 ") + (IsElevated() ? "是" : "否"));

    char folder[MAX_PATH] = {0};
    if (SaveEditor_GetSaveFolder(folder, sizeof(folder)) == 0) {
        lines.push_back(std::string("存档目录 ") + folder);
        int slots[64] = {0};
        int count = 0;
        if (SaveEditor_ScanSaves(slots, &count, 64) == 0)
            lines.push_back("存档数量 " + std::to_string(count));
        else
            lines.push_back("存档数量 读取失败");
    } else {
        lines.push_back("存档目录 读取失败");
    }

    TrainerStatus st;
    TrainerClient_Status(&st);
    lines.push_back(std::string("游戏进程 ") + (st.gameRunning ? "已检测" : "未检测"));
    lines.push_back(std::string("核心连接 ") + (st.attached ? "已附加" : "未附加"));

    if (st.attached) {
        std::string reply;
        std::string error;
        if (TrainerClient_Command("SELFCHECK", &reply, &error))
            lines.push_back("核心自检 " + reply);
        else
            lines.push_back("核心自检 " + error);
    }
    lines.push_back("Obtain All 数量需要在游戏内逐项实测");

    std::string body = "{\"ok\":true,\"lines\":[";
    for (size_t i = 0; i < lines.size(); i++) {
        if (i) body += ",";
        body += "\"" + JsonEscape(lines[i]) + "\"";
    }
    body += "]}";
    return body;
}

std::string HandleApi(const std::string& rawPath, const std::string& body)
{
    std::lock_guard<std::mutex> guard(g_apiLock);
    std::string path = rawPath;
    size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);

    if (path == "/api/meta") return MetaJson();
    if (path == "/api/saves") return SavesJson();
    if (path == "/api/bosses") return BossesJson();

    if (path == "/api/save")
    {
        int slot = QueryInt(rawPath, "slot", -1);
        if (slot < 0) return Result(-1);
        int fund = ReadSlotFund(slot);
        if (fund < 0) return Result(-1);
        return "{\"ok\":true,\"slot\":" + std::to_string(slot) + ",\"fund\":" + std::to_string(fund) + "}";
    }

    if (path == "/api/bg")
    {
        std::string data = GetStringField(body, "data");
        size_t comma = data.find(',');
        if (comma == std::string::npos) return Result(-9);
        std::string bytes;
        if (!Base64Decode(data.substr(comma + 1), bytes)) return Result(-9);
        CreateDirectoryA((g_root + "\\assets").c_str(), NULL);
        if (!WriteFileBytes(g_customBg, bytes)) return Result(-4);
        return Result(0, "背景已更新");
    }

    if (path == "/api/bg/reset")
    {
        DeleteFileA(g_customBg.c_str());
        return Result(0, "已恢复默认背景");
    }

    if (path == "/api/fund")
    {
        int slot = GetIntField(body, "slot", -1);
        int value = GetIntField(body, "value", -1);
        if (slot < 0 || value < 0) return Result(-1);
        std::string file;
        if (!SlotPath(slot, file)) return Result(-1);
        int ret = SaveEditor_SetFund(file.c_str(), value);
        return Result(ret, ret == 0 ? "完成（金钱已写入 " + std::to_string(value) + "）" : "");
    }

    if (path == "/api/maps")
    {
        int slot = GetIntField(body, "slot", -1);
        if (slot < 0) return Result(-1);
        std::string file;
        if (!SlotPath(slot, file)) return Result(-1);
        int ret = SaveEditor_UnlockAllMaps(file.c_str());
        if (ret >= 0)
            return Result(ret, "完成（新解锁 " + std::to_string(ret) + " / " + std::to_string(SaveEditor_GetMapCount()) + " 张地图）");
        return Result(ret);
    }

    if (path == "/api/bonds/list")
    {
        int slot = QueryInt(rawPath, "slot", -1);
        if (slot < 0) return Result(-1);
        std::string file;
        if (!SlotPath(slot, file)) return Result(-1);

        int ids[256] = {0};
        int levels[256] = {0};
        int total = SaveEditor_ListBonds(file.c_str(), ids, levels, 256);
        if (total < 0) return Result(total);

        std::string body = "{\"ok\":true,\"slot\":" + std::to_string(slot) + ",\"bonds\":[";
        int count = total < 256 ? total : 256;
        for (int i = 0; i < count; i++)
        {
            if (i) body += ",";
            body += "{\"id\":" + std::to_string(ids[i]) + ",\"level\":" + std::to_string(levels[i]) + "}";
        }
        body += "]}";
        return body;
    }

    if (path == "/api/bonds/set")
    {
        int slot = GetIntField(body, "slot", -1);
        int level = GetIntField(body, "level", 5);
        if (slot < 0 || level < 0 || level > 5) return Result(-1, "角色等级参数不正确");
        std::vector<int> ids;
        GetIntArrayField(body, "ids", ids);
        if (ids.empty()) return Result(-1, "没有选中角色");
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        if (ids.size() > 256) return Result(-1, "一次最多写入 256 个角色");
        std::string file;
        if (!SlotPath(slot, file)) return Result(-1);
        int ret = SaveEditor_SetBondLevels(file.c_str(), ids.data(), (int)ids.size(), level);
        if (ret == 0) return Result(-1, "没有匹配到角色");
        if (ret < 0) return Result(ret);
        return Result(0, "完成（已写入 " + std::to_string(ret) + " 个角色）");
    }

    if (path == "/api/bonds")
    {
        int slot = GetIntField(body, "slot", -1);
        if (slot < 0) return Result(-1);
        std::string file;
        if (!SlotPath(slot, file)) return Result(-1);
        int ret = SaveEditor_MaxAllBonds(file.c_str());
        if (ret >= 0)
            return Result(ret, "完成（" + std::to_string(ret) + " 个角色好感已满）");
        if (ret == -8) return Result(ret, "该存档里没有好感数据");
        return Result(ret);
    }

    if (path == "/api/boss/clear" || path == "/api/boss/queue")
    {
        int slot = GetIntField(body, "slot", -1);
        int boss = GetIntField(body, "boss", -1);
        if (slot < 0 || boss < 0) return Result(-1);
        std::string file;
        if (!SlotPath(slot, file)) return Result(-1);
        int ret = path == "/api/boss/clear" ? SaveEditor_SetBossCleared(file.c_str(), boss)
                                            : SaveEditor_QueueBossEvents(file.c_str(), boss);
        return Result(ret);
    }

    if (path == "/api/boss/invite")
    {
        int slot = GetIntField(body, "slot", -1);
        if (slot < 0) return Result(-1);
        return Result(SaveEditor_AddInvitationsToSlot(slot));
    }

    if (path == "/api/trainer/status")
    {
        return TrainerStatusJson();
    }

    if (path == "/api/trainer/feature")
    {
        std::string key = GetStringField(body, "key");
        bool enabled = GetBoolField(body, "enabled", false);

        if (key.compare(0, 7, "obtain:") == 0)
        {
            std::string category = key.substr(7);
            static const char* kAllowed[] = {
                "badges", "clothes", "records", "fishing", "partners", "decorations",
                "ingredients", "foods", "beverages", "cookers", "recipes", "izakayas"
            };
            bool allowed = false;
            for (size_t i = 0; i < sizeof(kAllowed) / sizeof(kAllowed[0]); i++)
            {
                if (category == kAllowed[i]) allowed = true;
            }
            if (!allowed) return Result(-1, "未知开关");
            if (!enabled) return Result(0, "已关闭");

            std::string reply;
            std::string error;
            if (!TrainerClient_Command("OBTAIN " + category, &reply, &error)) return Result(-1, error);
            if (reply.compare(0, 2, "OK") != 0) return Result(-1, CommandError(reply));
            return Result(0, "已执行");
        }

        if (key == "freeze_time")
        {
            std::string reply;
            std::string error;
            std::string command = std::string("FREEZE ") + (enabled ? "1" : "0");
            if (!TrainerClient_Command(command, &reply, &error)) return Result(-1, error);
            if (reply.compare(0, 2, "OK") != 0) return Result(-1, CommandError(reply));
            return Result(0, enabled ? "已暂停" : "已恢复");
        }

        return Result(-1, "未知开关");
    }

    if (path == "/api/selfcheck")
    {
        return SelfCheckJson();
    }

    if (path == "/api/trainer/attach")
    {
        std::string error;
        if (!TrainerClient_Attach(&error)) return Result(-1, error);
        return Result(0, "已附加游戏");
    }

    if (path == "/api/trainer/detach")
    {
        std::string reply;
        std::string error;
        TrainerClient_Command("FREEZE 0", &reply, &error);
        TrainerClient_Detach();
        return Result(0, "已卸下");
    }

    if (path == "/api/trainer/money")
    {
        long long value = GetInt64Field(body, "value", -1);
        if (value < 0) return Result(-1, "金额不正确");
        std::string reply;
        std::string error;
        if (!TrainerClient_Command("MONEY SET " + std::to_string(value), &reply, &error)) return Result(-1, error);
        if (reply.compare(0, 2, "OK") != 0) return Result(-1, CommandError(reply));
        return Result(0, "实时金钱已写入 " + std::to_string(value));
    }

    if (path == "/api/trainer/speed")
    {
        double value = GetFloatField(body, "value", -1.0);
        if (value < 0.1 || value > 10.0) return Result(-1, "速度范围 0.1 到 10");
        char command[64] = {0};
        _snprintf(command, sizeof(command) - 1, "SPEED SET %.4f", value);
        std::string reply;
        std::string error;
        if (!TrainerClient_Command(command, &reply, &error)) return Result(-1, error);
        if (reply.compare(0, 2, "OK") != 0) return Result(-1, CommandError(reply));
        return Result(0, "游戏速度已设置");
    }

    if (path == "/api/trainer/obtain")
    {
        std::string category = GetStringField(body, "category");
        static const char* kAllowed[] = {
            "badges", "clothes", "records", "fishing", "partners", "decorations",
            "ingredients", "foods", "beverages", "cookers", "recipes", "izakayas"
        };
        bool allowed = false;
        for (size_t i = 0; i < sizeof(kAllowed) / sizeof(kAllowed[0]); i++)
        {
            if (category == kAllowed[i]) allowed = true;
        }
        if (!allowed) return Result(-1, "未知项目");
        std::string reply;
        std::string error;
        if (!TrainerClient_Command("OBTAIN " + category, &reply, &error)) return Result(-1, error);
        if (reply.compare(0, 2, "OK") != 0) return Result(-1, CommandError(reply));
        return Result(0, "已执行");
    }

    return "{\"ok\":false,\"code\":-404,\"msg\":\"unknown api\"}";
}

std::string ContentType(const std::string& path)
{
    struct Ext { const char* ext; const char* type; };
    static const Ext kTypes[] = {
        {".html", "text/html; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".js", "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".webp", "image/webp"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".woff2", "font/woff2"},
        {".txt", "text/plain; charset=utf-8"}
    };
    for (size_t i = 0; i < sizeof(kTypes) / sizeof(kTypes[0]); i++)
    {
        size_t n = strlen(kTypes[i].ext);
        if (path.size() >= n && _stricmp(path.c_str() + path.size() - n, kTypes[i].ext) == 0)
            return kTypes[i].type;
    }
    return "application/octet-stream";
}

bool ServeStatic(const std::string& rawPath, std::string& body, std::string& type)
{
    std::string rel = rawPath;
    size_t q = rel.find('?');
    if (q != std::string::npos) rel = rel.substr(0, q);
    if (rel.empty() || rel == "/") rel = "/index.html";
    if (rel.find("..") != std::string::npos) return false;
    std::string file = g_root;
    for (size_t i = 0; i < rel.size(); i++) file += (rel[i] == '/' ? '\\' : rel[i]);
    if (!ReadFileBytes(file, body)) return false;
    type = ContentType(rel);
    return true;
}

struct Request
{
    std::string method;
    std::string target;
    std::string body;
    bool isJson;

    Request() : isJson(false) {}
};

bool RecvRequest(SOCKET c, Request& req)
{
    std::string buf;
    char tmp[8192];
    size_t headerEnd = std::string::npos;
    size_t contentLength = 0;
    const size_t kMax = 24u * 1024u * 1024u;
    for (;;)
    {
        int n = recv(c, tmp, (int)sizeof(tmp), 0);
        if (n <= 0) break;
        buf.append(tmp, (size_t)n);
        if (headerEnd == std::string::npos)
        {
            headerEnd = buf.find("\r\n\r\n");
            if (headerEnd != std::string::npos)
            {
                std::string head = buf.substr(0, headerEnd);
                size_t p = head.find("Content-Length:");
                if (p == std::string::npos) p = head.find("content-length:");
                if (p != std::string::npos) contentLength = (size_t)strtoul(head.c_str() + p + 15, NULL, 10);

                req.isJson = false;
                size_t ct = head.find("Content-Type:");
                if (ct == std::string::npos) ct = head.find("content-type:");
                if (ct != std::string::npos) {
                    size_t v = ct + 13;
                    while (v < head.size() && (head[v] == ' ' || head[v] == '\t')) v++;
                    if (_strnicmp(head.c_str() + v, "application/json", 16) == 0) req.isJson = true;
                }
            }
        }
        if (headerEnd != std::string::npos && buf.size() >= headerEnd + 4 + contentLength) break;
        if (buf.size() > kMax) break;
    }
    if (headerEnd == std::string::npos) return false;
    std::string head = buf.substr(0, headerEnd);
    size_t sp1 = head.find(' ');
    size_t sp2 = sp1 == std::string::npos ? std::string::npos : head.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
    req.method = head.substr(0, sp1);
    req.target = head.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t bodyStart = headerEnd + 4;
    req.body = buf.size() > bodyStart ? buf.substr(bodyStart) : std::string();
    return true;
}

void SendAll(SOCKET c, const std::string& text)
{
    const char* p = text.data();
    size_t left = text.size();
    while (left > 0)
    {
        int n = send(c, p, (int)left, 0);
        if (n <= 0) break;
        p += n;
        left -= (size_t)n;
    }
}

void SendResponse(SOCKET c, int status, const std::string& type, const std::string& body)
{
    const char* reason = "Not Found";
    if (status == 200) reason = "OK";
    else if (status == 400) reason = "Bad Request";
    else if (status == 413) reason = "Payload Too Large";
    else if (status == 415) reason = "Unsupported Media Type";
    std::string head = "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
    head += "Content-Type: " + type + "\r\n";
    head += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    head += "Cache-Control: no-store\r\n";
    head += "Connection: close\r\n\r\n";
    SendAll(c, head + body);
}

void HandleClient(SOCKET c)
{
    Request req;
    if (RecvRequest(c, req))
    {
        if (req.method == "POST" && !req.isJson)
        {
            SendResponse(c, 415, "application/json; charset=utf-8",
                         "{\"ok\":false,\"code\":-415,\"msg\":\"Content-Type 必须是 application/json\"}");
        }
        else if (req.target.compare(0, 5, "/api/") == 0)
        {
            std::string body = HandleApi(req.target, req.body);
            SendResponse(c, 200, "application/json; charset=utf-8", body);
        }
        else
        {
            std::string body;
            std::string type;
            if (ServeStatic(req.target, body, type))
                SendResponse(c, 200, type, body);
            else
                SendResponse(c, 404, "text/plain; charset=utf-8", "404 Not Found");
        }
    }
    closesocket(c);
}

void AcceptLoop(SOCKET listener)
{
    while (g_running)
    {
        sockaddr_in addr;
        int len = (int)sizeof(addr);
        SOCKET c = accept(listener, (sockaddr*)&addr, &len);
        if (c == INVALID_SOCKET)
        {
            if (!g_running) break;
            continue;
        }
        std::thread(HandleClient, c).detach();
    }
}
}

bool WebServer::Start(int preferredPort)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    g_root = ExeDir() + "\\web";
    if (GetFileAttributesA((g_root + "\\index.html").c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        std::string alt = ExeDir() + "\\..\\web";
        if (GetFileAttributesA((alt + "\\index.html").c_str()) != INVALID_FILE_ATTRIBUTES) g_root = alt;
    }
    g_customBg = g_root + "\\assets\\custom.jpg";

    for (int i = 0; i < 12; i++)
    {
        int port = preferredPort + i;
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) break;
        BOOL reuse = TRUE;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((u_short)port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        if (bind(s, (sockaddr*)&addr, sizeof(addr)) == 0 && listen(s, 32) == 0)
        {
            g_listen = s;
            g_port = port;
            g_running = true;
            g_acceptThread = std::thread(AcceptLoop, s);
            return true;
        }
        closesocket(s);
    }

    WSACleanup();
    return false;
}

void WebServer::Stop()
{
    if (!g_running.exchange(false)) return;
    if (g_listen != INVALID_SOCKET)
    {
        closesocket(g_listen);
        g_listen = INVALID_SOCKET;
    }
    if (g_acceptThread.joinable()) g_acceptThread.join();
    WSACleanup();
}

int WebServer::Port()
{
    return g_port;
}
