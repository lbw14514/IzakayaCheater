#pragma once
#include <string>

struct TrainerStatus
{
    bool gameRunning;
    bool attached;
    bool resolved;
    bool moneyReady;
    bool speedReady;
    bool frozen;
    long long money;
    float timeScale;
    std::string error;

    TrainerStatus() : gameRunning(false), attached(false), resolved(false),
                      moneyReady(false), speedReady(false), frozen(false), money(0), timeScale(1.0f) {}
};

bool TrainerClient_Status(TrainerStatus* out);
bool TrainerClient_Attach(std::string* error);
void TrainerClient_Detach();
bool TrainerClient_Command(const std::string& command, std::string* reply, std::string* error);
