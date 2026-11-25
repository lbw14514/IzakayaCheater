#pragma once
#include <cstddef>
#include <string>
#include <vector>

struct BackupEntry
{
    int id;
    std::string time;
    std::string kind;
    unsigned long long size;
    unsigned long long storedSize;
    std::string sha256;
};

bool Backup_List(const char* savePath, std::vector<BackupEntry>& entries, std::string* error);
bool Backup_BeforeWrite(const char* savePath, const char* data, size_t size, std::string* error);
bool Backup_Restore(const char* savePath, int id, std::string* error);
bool Backup_Snapshot(const char* savePath, std::string* error);
