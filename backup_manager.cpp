#include "backup_manager.h"
#include "json_lite.h"

#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using Bytes = std::vector<unsigned char>;

const char* kIndexName = "index.txt";
const char* kBaseName = "base.bin";
const size_t kBlockSize = 32;
const unsigned long long kLargeLiteral = 1u << 20;

struct IndexEntry
{
    int id;
    std::string time;
    std::string kind;
    std::string file;
    unsigned long long size;
    std::string sha;
};

struct BackupIndex
{
    std::string dir;
    std::string baseTime;
    unsigned long long baseSize;
    std::string baseSha;
    std::vector<IndexEntry> entries;

    BackupIndex() : baseSize(0) {}
};

std::string NowString()
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[64] = {0};
    _snprintf(buf, sizeof(buf) - 1, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

std::string FileTimeString(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) return NowString();
    FILETIME local;
    if (!FileTimeToLocalFileTime(&data.ftLastWriteTime, &local)) return NowString();
    SYSTEMTIME st;
    if (!FileTimeToSystemTime(&local, &st)) return NowString();
    char buf[64] = {0};
    _snprintf(buf, sizeof(buf) - 1, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return buf;
}

bool EnsureDirectory(const std::string& path, std::string* error)
{
    if (path.empty()) return true;
    if (CreateDirectoryA(path.c_str(), NULL)) return true;
    if (GetLastError() == ERROR_ALREADY_EXISTS) return true;
    if (error) *error = "无法创建备份目录 " + path;
    return false;
}

std::string BackupRoot()
{
    char local[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local))) {
        return std::string(local) + "\\IzakayaCheater\\save_backups";
    }
    DWORD size = GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH);
    if (size > 0 && size < MAX_PATH) {
        return std::string(local) + "\\IzakayaCheater\\save_backups";
    }
    return "";
}

std::string BaseName(const std::string& path)
{
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool FileExists(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

unsigned long long FileSize(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    return ((unsigned long long)data.nFileSizeHigh << 32) | data.nFileSizeLow;
}

bool ReadAll(const std::string& path, Bytes& out, std::string* error)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        if (error) *error = "无法读取 " + path;
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        if (error) *error = "无法读取 " + path;
        return false;
    }
    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        if (error) *error = "无法读取 " + path;
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        if (error) *error = "无法读取 " + path;
        return false;
    }
    out.resize((size_t)length);
    size_t read = length > 0 ? fread(out.data(), 1, (size_t)length, file) : 0;
    fclose(file);
    if (read != (size_t)length) {
        if (error) *error = "读取不完整 " + path;
        return false;
    }
    return true;
}

bool WriteAll(const std::string& path, const Bytes& data, std::string* error)
{
    FILE* file = fopen(path.c_str(), "wb");
    if (!file) {
        if (error) *error = "无法写入 " + path;
        return false;
    }
    size_t written = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), file);
    int closed = fclose(file);
    if (written != data.size() || closed != 0) {
        DeleteFileA(path.c_str());
        if (error) *error = "写入失败 " + path;
        return false;
    }
    return true;
}

bool WriteAtomic(const std::string& path, const Bytes& data, std::string* error)
{
    std::string temp = path + ".ic.restore.tmp";
    if (!WriteAll(temp, data, error)) return false;
    if (!MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(temp.c_str());
        if (error) *error = "替换存档失败";
        return false;
    }
    return true;
}

bool Sha256(const Bytes& data, std::string& out, std::string* error)
{
    static unsigned char emptyByte = 0;
    unsigned char digest[32] = {0};
    PUCHAR input = data.empty() ? &emptyByte : (PUCHAR)data.data();

    BCRYPT_ALG_HANDLE algorithm = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    std::vector<UCHAR> object;
    DWORD objectLength = 0;
    DWORD resultLength = 0;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                   (PUCHAR)&objectLength, sizeof(objectLength),
                                   &resultLength, 0);
    }
    if (BCRYPT_SUCCESS(status)) {
        object.resize(objectLength);
        status = BCryptCreateHash(algorithm, &hash, object.empty() ? NULL : object.data(),
                                  objectLength, NULL, 0, 0);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptHashData(hash, input, (ULONG)data.size(), 0);
    }
    if (BCRYPT_SUCCESS(status)) {
        status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!BCRYPT_SUCCESS(status)) {
        if (error) *error = "计算 SHA256 失败";
        return false;
    }

    static const char* hex = "0123456789abcdef";
    out.clear();
    out.reserve(64);
    for (unsigned char byte : digest) {
        out.push_back(hex[byte >> 4]);
        out.push_back(hex[byte & 0x0F]);
    }
    return true;
}

void AppendU32(Bytes& out, uint32_t value)
{
    for (int i = 0; i < 4; i++) out.push_back((unsigned char)(value >> (i * 8)));
}

void AppendU64(Bytes& out, uint64_t value)
{
    for (int i = 0; i < 8; i++) out.push_back((unsigned char)(value >> (i * 8)));
}

bool ReadU32(const Bytes& data, size_t& pos, uint32_t& value)
{
    if (pos + 4 > data.size()) return false;
    value = 0;
    for (int i = 0; i < 4; i++) value |= (uint32_t)data[pos + i] << (i * 8);
    pos += 4;
    return true;
}

bool ReadU64(const Bytes& data, size_t& pos, uint64_t& value)
{
    if (pos + 8 > data.size()) return false;
    value = 0;
    for (int i = 0; i < 8; i++) value |= (uint64_t)data[pos + i] << (i * 8);
    pos += 8;
    return true;
}

uint64_t BlockHash(const unsigned char* data, size_t length)
{
    uint64_t hash = 1469598103934665603ULL;
    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

void FlushLiteral(Bytes& delta, Bytes& literal)
{
    if (literal.empty()) return;
    delta.push_back(2);
    AppendU64(delta, (uint64_t)literal.size());
    delta.insert(delta.end(), literal.begin(), literal.end());
    literal.clear();
}

bool MakeDelta(const Bytes& base, const Bytes& target, Bytes& delta, std::string* error)
{
    delta.clear();
    delta.push_back('I');
    delta.push_back('C');
    delta.push_back('D');
    delta.push_back('1');
    AppendU32(delta, 1);
    AppendU64(delta, (uint64_t)base.size());
    AppendU64(delta, (uint64_t)target.size());

    std::unordered_map<uint64_t, size_t> index;
    if (base.size() >= kBlockSize) {
        index.reserve(base.size());
        for (size_t i = 0; i + kBlockSize <= base.size(); i++) {
            uint64_t hash = BlockHash(base.data() + i, kBlockSize);
            if (index.find(hash) == index.end()) index[hash] = i;
        }
    }

    Bytes literal;
    size_t pos = 0;
    while (pos < target.size()) {
        bool copied = false;
        if (pos + kBlockSize <= target.size()) {
            uint64_t hash = BlockHash(target.data() + pos, kBlockSize);
            auto found = index.find(hash);
            if (found != index.end()) {
                size_t offset = found->second;
                if (offset + kBlockSize <= base.size() &&
                    memcmp(base.data() + offset, target.data() + pos, kBlockSize) == 0) {
                    size_t length = kBlockSize;
                    while (pos + length < target.size() && offset + length < base.size() &&
                           target[pos + length] == base[offset + length]) {
                        length++;
                    }
                    FlushLiteral(delta, literal);
                    delta.push_back(1);
                    AppendU64(delta, (uint64_t)offset);
                    AppendU64(delta, (uint64_t)length);
                    pos += length;
                    copied = true;
                }
            }
        }
        if (!copied) {
            literal.push_back(target[pos]);
            pos++;
            if (literal.size() >= kLargeLiteral) FlushLiteral(delta, literal);
        }
    }
    FlushLiteral(delta, literal);
    delta.push_back(0);
    if (delta.size() > target.size() + 32 && target.size() > 0) {
        // 大改动时全量文件更小
        delta.clear();
        if (error) error->clear();
        return false;
    }
    if (error) error->clear();
    return true;
}

bool ApplyDelta(const Bytes& base, const Bytes& delta, Bytes& target, std::string* error)
{
    if (delta.size() < 25 || memcmp(delta.data(), "ICD1", 4) != 0) {
        if (error) *error = "增量文件头不正确";
        return false;
    }
    size_t pos = 4;
    uint32_t version = 0;
    uint64_t baseSize = 0;
    uint64_t targetSize = 0;
    if (!ReadU32(delta, pos, version) || version != 1 ||
        !ReadU64(delta, pos, baseSize) || !ReadU64(delta, pos, targetSize)) {
        if (error) *error = "增量文件头损坏";
        return false;
    }
    if (baseSize != base.size()) {
        if (error) *error = "增量与初始备份不匹配";
        return false;
    }
    target.clear();
    target.reserve((size_t)targetSize);
    while (pos < delta.size()) {
        unsigned char type = delta[pos++];
        if (type == 0) break;
        if (type == 1) {
            uint64_t offset = 0;
            uint64_t length = 0;
            if (!ReadU64(delta, pos, offset) || !ReadU64(delta, pos, length)) {
                if (error) *error = "增量复制指令损坏";
                return false;
            }
            if (offset > base.size() || length > base.size() - (size_t)offset) {
                if (error) *error = "增量复制越界";
                return false;
            }
            target.insert(target.end(), base.begin() + (size_t)offset,
                          base.begin() + (size_t)(offset + length));
        } else if (type == 2) {
            uint64_t length = 0;
            if (!ReadU64(delta, pos, length) || length > delta.size() - pos) {
                if (error) *error = "增量字面量损坏";
                return false;
            }
            target.insert(target.end(), delta.begin() + pos, delta.begin() + pos + (size_t)length);
            pos += (size_t)length;
        } else {
            if (error) *error = "未知增量指令";
            return false;
        }
    }
    if (target.size() != (size_t)targetSize) {
        if (error) *error = "增量还原大小不一致";
        return false;
    }
    return true;
}

std::string IndexPath(const std::string& dir)
{
    return dir + "\\" + kIndexName;
}

bool SaveIndex(const BackupIndex& index, std::string* error)
{
    std::string text = "ICBK1\n";
    text += "basetime=" + index.baseTime + "\n";
    text += "basesize=" + std::to_string(index.baseSize) + "\n";
    text += "basesha=" + index.baseSha + "\n";
    for (const IndexEntry& entry : index.entries) {
        text += "E|" + std::to_string(entry.id) + "|" + entry.time + "|" + entry.kind + "|" +
                entry.file + "|" + std::to_string(entry.size) + "|" + entry.sha + "\n";
    }
    Bytes data(text.begin(), text.end());
    if (!WriteAll(IndexPath(index.dir), data, error)) return false;
    return true;
}

bool Split(const std::string& line, char sep, std::vector<std::string>& parts)
{
    parts.clear();
    size_t start = 0;
    for (;;) {
        size_t pos = line.find(sep, start);
        if (pos == std::string::npos) {
            parts.push_back(line.substr(start));
            break;
        }
        parts.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
    return true;
}

bool LoadIndex(const std::string& dir, BackupIndex& index, std::string* error)
{
    index = BackupIndex();
    index.dir = dir;
    Bytes data;
    if (!ReadAll(IndexPath(dir), data, error)) return false;
    std::string text(data.begin(), data.end());
    size_t lineStart = 0;
    bool header = false;
    while (lineStart <= text.size()) {
        size_t lineEnd = text.find('\n', lineStart);
        std::string line = text.substr(lineStart, lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart);
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (!header) {
            if (line != "ICBK1") {
                if (error) *error = "备份索引版本不正确";
                return false;
            }
            header = true;
        } else if (!line.empty()) {
            if (line.compare(0, 9, "basetime=") == 0) {
                index.baseTime = line.substr(9);
            } else if (line.compare(0, 9, "basesize=") == 0) {
                index.baseSize = strtoull(line.substr(9).c_str(), NULL, 10);
            } else if (line.compare(0, 8, "basesha=") == 0) {
                index.baseSha = line.substr(8);
            } else if (line.compare(0, 2, "E|") == 0) {
                std::vector<std::string> parts;
                Split(line, '|', parts);
                if (parts.size() == 7) {
                    IndexEntry entry;
                    entry.id = atoi(parts[1].c_str());
                    entry.time = parts[2];
                    entry.kind = parts[3];
                    entry.file = parts[4];
                    entry.size = strtoull(parts[5].c_str(), NULL, 10);
                    entry.sha = parts[6];
                    index.entries.push_back(entry);
                }
            }
        }
        if (lineEnd == std::string::npos) break;
        lineStart = lineEnd + 1;
    }
    if (!header || index.entries.empty()) {
        if (error) *error = "备份索引为空";
        return false;
    }
    return true;
}

bool InitBaseline(BackupIndex& index, const Bytes& data, const std::string& time, std::string* error)
{
    if (!WriteAll(index.dir + "\\" + kBaseName, data, error)) return false;
    index.baseTime = time;
    index.baseSize = (unsigned long long)data.size();
    if (!Sha256(data, index.baseSha, error)) return false;
    index.entries.clear();
    IndexEntry entry;
    entry.id = 0;
    entry.time = time;
    entry.kind = "initial";
    entry.file = kBaseName;
    entry.size = index.baseSize;
    entry.sha = index.baseSha;
    index.entries.push_back(entry);
    return SaveIndex(index, error);
}

bool FindLegacy(const std::string& savePath, std::vector<std::string>& files)
{
    files.clear();
    std::string root = BackupRoot();
    if (root.empty()) return false;
    std::string base = BaseName(savePath);
    std::string pattern = root + "\\" + base + ".*.bak";
    WIN32_FIND_DATAA data;
    HANDLE find = FindFirstFileA(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return false;
    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            files.push_back(root + "\\" + data.cFileName);
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    std::sort(files.begin(), files.end());
    return !files.empty();
}

bool MigrateLegacy(const std::string& savePath, BackupIndex& index, std::string* error)
{
    std::vector<std::string> legacy;
    if (!FindLegacy(savePath, legacy) || legacy.empty()) return false;
    Bytes first;
    if (!ReadAll(legacy[0], first, error)) return false;
    if (!InitBaseline(index, first, FileTimeString(legacy[0]), error)) return false;

    int nextId = 1;
    for (size_t i = 1; i < legacy.size(); i++) {
        Bytes data;
        if (!ReadAll(legacy[i], data, error)) return false;
        char name[64] = {0};
        _snprintf(name, sizeof(name) - 1, "legacy_%04d.bin", nextId);
        if (!WriteAll(index.dir + "\\" + name, data, error)) return false;
        std::string sha;
        if (!Sha256(data, sha, error)) return false;
        IndexEntry entry;
        entry.id = nextId++;
        entry.time = FileTimeString(legacy[i]);
        entry.kind = "full";
        entry.file = name;
        entry.size = (unsigned long long)data.size();
        entry.sha = sha;
        index.entries.push_back(entry);
    }
    if (!SaveIndex(index, error)) return false;
    for (const std::string& path : legacy) DeleteFileA(path.c_str());
    return true;
}

bool EnsureIndex(const std::string& savePath, BackupIndex& index, bool createBaseline, std::string* error)
{
    std::string root = BackupRoot();
    if (root.empty()) {
        if (error) *error = "无法定位 LOCALAPPDATA";
        return false;
    }
    size_t parentSlash = root.find_last_of("\\/");
    if (parentSlash != std::string::npos && !EnsureDirectory(root.substr(0, parentSlash), error)) {
        return false;
    }
    std::string dir = root + "\\" + BaseName(savePath);
    if (!EnsureDirectory(root, error) || !EnsureDirectory(dir, error)) return false;
    index.dir = dir;

    if (FileExists(IndexPath(dir))) {
        return LoadIndex(dir, index, error);
    }

    if (MigrateLegacy(savePath, index, error)) return true;

    if (!createBaseline) {
        index.entries.clear();
        return true;
    }

    Bytes current;
    if (FileExists(savePath)) {
        if (!ReadAll(savePath, current, error)) return false;
    }
    return InitBaseline(index, current, NowString(), error);
}

bool RecordBytes(BackupIndex& index, const Bytes& data, const std::string& time, std::string* error)
{
    std::string sha;
    if (!Sha256(data, sha, error)) return false;
    if (!index.entries.empty() && index.entries.back().sha == sha) return true;

    Bytes base;
    if (!ReadAll(index.dir + "\\" + kBaseName, base, error)) return false;
    Bytes delta;
    bool useDelta = MakeDelta(base, data, delta, error);
    int nextId = index.entries.empty() ? 1 : index.entries.back().id + 1;
    char name[64] = {0};
    IndexEntry entry;
    entry.id = nextId;
    entry.time = time;
    entry.size = (unsigned long long)data.size();
    entry.sha = sha;

    if (useDelta && delta.size() < data.size() + 1) {
        _snprintf(name, sizeof(name) - 1, "v%06d.delta", nextId);
        entry.kind = "delta";
        if (!WriteAll(index.dir + "\\" + name, delta, error)) return false;
    } else {
        _snprintf(name, sizeof(name) - 1, "v%06d.full", nextId);
        entry.kind = "full";
        if (!WriteAll(index.dir + "\\" + name, data, error)) return false;
    }
    entry.file = name;
    index.entries.push_back(entry);
    return SaveIndex(index, error);
}

bool RecordCurrentFile(BackupIndex& index, const std::string& savePath, std::string* error)
{
    if (!FileExists(savePath)) return true;
    Bytes current;
    if (!ReadAll(savePath, current, error)) return false;
    return RecordBytes(index, current, NowString(), error);
}

bool ReadEntryData(const BackupIndex& index, const IndexEntry& entry, Bytes& data, std::string* error)
{
    if (entry.kind == "initial") {
        return ReadAll(index.dir + "\\" + kBaseName, data, error);
    }
    if (entry.kind == "full") {
        return ReadAll(index.dir + "\\" + entry.file, data, error);
    }
    if (entry.kind != "delta") {
        if (error) *error = "未知备份类型";
        return false;
    }
    Bytes base;
    Bytes delta;
    if (!ReadAll(index.dir + "\\" + kBaseName, base, error)) return false;
    if (!ReadAll(index.dir + "\\" + entry.file, delta, error)) return false;
    return ApplyDelta(base, delta, data, error);
}
}

bool Backup_List(const char* savePath, std::vector<BackupEntry>& entries, std::string* error)
{
    entries.clear();
    if (!savePath) {
        if (error) *error = "存档路径为空";
        return false;
    }
    BackupIndex index;
    if (!EnsureIndex(savePath, index, false, error)) return false;
    for (const IndexEntry& entry : index.entries) {
        BackupEntry out;
        out.id = entry.id;
        out.time = entry.time;
        out.kind = entry.kind;
        out.size = entry.size;
        out.storedSize = FileSize(index.dir + "\\" + (entry.kind == "initial" ? kBaseName : entry.file));
        out.sha256 = entry.sha;
        entries.push_back(out);
    }
    return true;
}

bool Backup_BeforeWrite(const char* savePath, const char* data, size_t size, std::string* error)
{
    if (!savePath || (!data && size > 0)) {
        if (error) *error = "备份参数不合法";
        return false;
    }
    BackupIndex index;
    if (!EnsureIndex(savePath, index, true, error)) return false;
    if (!RecordCurrentFile(index, savePath, error)) return false;
    Bytes next;
    if (size > 0) next.assign((const unsigned char*)data, (const unsigned char*)data + size);
    if (!RecordBytes(index, next, NowString(), error)) return false;
    return SaveIndex(index, error);
}

bool Backup_Restore(const char* savePath, int id, std::string* error)
{
    if (!savePath) {
        if (error) *error = "存档路径为空";
        return false;
    }
    BackupIndex index;
    if (!EnsureIndex(savePath, index, false, error)) return false;
    const IndexEntry* target = NULL;
    for (const IndexEntry& entry : index.entries) {
        if (entry.id == id) target = &entry;
    }
    if (!target) {
        if (error) *error = "找不到该备份";
        return false;
    }

    Bytes data;
    if (!ReadEntryData(index, *target, data, error)) return false;
    std::string sha;
    if (!Sha256(data, sha, error)) return false;
    if (data.size() != (size_t)target->size || sha != target->sha) {
        if (error) *error = "备份文件校验失败";
        return false;
    }
    std::string jsonError;
    if (!Json_Validate((const char*)data.data(), data.size(), &jsonError)) {
        if (error) *error = "该备份不是完整 JSON 已拒绝恢复";
        return false;
    }

    if (FileExists(savePath)) {
        Bytes current;
        if (ReadAll(savePath, current, error)) {
            std::string currentSha;
            if (Sha256(current, currentSha, error) && currentSha == sha) return true;
        }
    }

    if (!RecordCurrentFile(index, savePath, error)) return false;
    if (!WriteAtomic(savePath, data, error)) return false;
    return SaveIndex(index, error);
}

bool Backup_Snapshot(const char* savePath, std::string* error)
{
    if (!savePath || !FileExists(savePath)) {
        if (error) *error = "当前没有可备份的存档";
        return false;
    }
    BackupIndex index;
    if (!EnsureIndex(savePath, index, true, error)) return false;
    Bytes current;
    if (!ReadAll(savePath, current, error)) return false;
    std::string sha;
    if (!Sha256(current, sha, error)) return false;
    if (!index.entries.empty() && index.entries.back().sha == sha) return true;

    int nextId = index.entries.empty() ? 1 : index.entries.back().id + 1;
    char name[64] = {0};
    _snprintf(name, sizeof(name) - 1, "snap_%06d.full", nextId);
    if (!WriteAll(index.dir + "\\" + name, current, error)) return false;

    IndexEntry entry;
    entry.id = nextId;
    entry.time = NowString();
    entry.kind = "full";
    entry.file = name;
    entry.size = (unsigned long long)current.size();
    entry.sha = sha;
    index.entries.push_back(entry);
    return SaveIndex(index, error);
}
