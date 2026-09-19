#include "web_server.h"
#include "save_editor.h"
#include "config.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace
{
std::atomic<bool> g_running(false);
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
    return atoi(body.c_str() + p + 1);
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
    return atoi(path.c_str() + p + needle.size());
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

std::string HandleApi(const std::string& rawPath, const std::string& body)
{
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
    std::string head = "HTTP/1.1 " + std::to_string(status) + (status == 200 ? " OK" : " Not Found") + "\r\n";
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
        if (req.target.compare(0, 5, "/api/") == 0)
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
