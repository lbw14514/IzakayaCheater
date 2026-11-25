#include "save_editor.h"
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Expected MD5 values are the v0.6.2 byte baseline.
static std::string Md5File(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    BCRYPT_ALG_HANDLE alg = NULL;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_MD5_ALGORITHM, NULL, 0) < 0) return std::string();
    unsigned char digest[16] = {0};
    NTSTATUS status = BCryptHash(alg, NULL, 0, data.data(), (ULONG)data.size(), digest, (ULONG)sizeof(digest));
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) return std::string();

    static const char* hex = "0123456789abcdef";
    std::string out;
    for (unsigned char b : digest)
    {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 15]);
    }
    return out;
}

static bool Check(const std::string& path, const std::string& name, const char* expected)
{
    std::string actual = Md5File(path);
    bool ok = actual == expected;
    std::cout << name << " " << actual << (ok ? " OK" : " FAIL") << "\n";
    return ok;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: save_editor_byte_test <fixture.memory>\n";
        return 2;
    }

    fs::path temp = fs::temp_directory_path() / ("izakaya-byte-" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(temp, ec);
    fs::create_directories(temp, ec);
    fs::path work = temp / "working.memory";
    fs::copy_file(argv[1], work, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        std::cerr << "copy fixture failed: " << ec.message() << "\n";
        return 2;
    }

    std::string path = work.string();
    bool ok = true;
    int ids[256] = {0};
    int levels[256] = {0};
    int ret = 0;

    ret = SaveEditor_SetFund(path.c_str(), 123456);
    std::cout << "01_fund=" << ret << "\n";
    ok &= Check(path, "01_fund", "ce22de01f79ddaaf6459ddbc53815949");

    ret = SaveEditor_UnlockAllMaps(path.c_str());
    std::cout << "02_maps=" << ret << "\n";
    ok &= Check(path, "02_maps", "8562a2c6598ba100c3df6c6e1e3529a9");

    ret = SaveEditor_MaxAllBonds(path.c_str());
    std::cout << "03_maxbonds=" << ret << "\n";
    ok &= Check(path, "03_maxbonds", "ce0f606bc197873fbdfdfa7a46a20a67");

    int bondCount = SaveEditor_ListBonds(path.c_str(), ids, levels, 256);
    ret = SaveEditor_SetBondLevels(path.c_str(), ids, bondCount >= 3 ? 3 : bondCount, 5);
    std::cout << "04_bondlevels=" << ret << "\n";
    ok &= Check(path, "04_bondlevels", "ce0f606bc197873fbdfdfa7a46a20a67");

    ret = SaveEditor_SetDLC2Bonds(path.c_str());
    std::cout << "05_dlc2bonds=" << ret << "\n";
    ok &= Check(path, "05_dlc2bonds", "ce0f606bc197873fbdfdfa7a46a20a67");

    ret = SaveEditor_SetDLC3Bonds(path.c_str());
    std::cout << "06_dlc3bonds=" << ret << "\n";
    ok &= Check(path, "06_dlc3bonds", "ce0f606bc197873fbdfdfa7a46a20a67");

    ret = SaveEditor_AddInvitations(path.c_str());
    std::cout << "07_invite=" << ret << "\n";
    ok &= Check(path, "07_invite", "d02460836896f3f83f8092c15310123e");

    ret = SaveEditor_AddInvitations(path.c_str());
    std::cout << "08_invite2=" << ret << "\n";
    ok &= Check(path, "08_invite2", "f28367e0ad210daba8df4312a6cf5256");

    static const char* queueHashes[6] = {
        "0733660aaca679e825be345e2dcaacdd",
        "c307158953d652f9f4cfb64eea970994",
        "9e032c4e05525cf28bbecfcdd8af01e0",
        "9f1f0ae47cf7de255bd919f68d352eb2",
        "ef3db56a283d3c4c2ef0a2326521a60e",
        "57fd79932ac06e1fce3a962913bf4116"
    };
    for (int i = 0; i < 6; i++)
    {
        ret = SaveEditor_QueueBossEvents(path.c_str(), i);
        std::cout << "queue" << i << "=" << ret << "\n";
        ok &= Check(path, "queue" + std::to_string(i), queueHashes[i]);
    }

    static const char* clearHashes[6] = {
        "0700982513ccbaaeb51ef26350e4d43b",
        "ae0b8332e73a4944a4f02aca4fde86f5",
        "2814ee1302afecba8c25d07dca4a162a",
        "c0294cd58c4175666d9123b7845b5605",
        "c682ea887b89ce09da85b66f7ffeb063",
        "e010e61e13a9c344647bd5877da2c847"
    };
    for (int i = 0; i < 6; i++)
    {
        ret = SaveEditor_SetBossCleared(path.c_str(), i);
        std::cout << "clear" << i << "=" << ret << "\n";
        ok &= Check(path, "clear" + std::to_string(i), clearHashes[i]);
    }

    ret = SaveEditor_TriggerFestival(path.c_str());
    std::cout << "21_festival=" << ret << "\n";
    ok &= Check(path, "21_festival", "a5912dcff81ee2057a226088ab8aa9e1");

    fs::remove_all(temp, ec);
    if (!ok)
    {
        std::cerr << "byte consistency failed\n";
        return 1;
    }
    std::cout << "21/21 byte consistency OK\n";
    return 0;
}
