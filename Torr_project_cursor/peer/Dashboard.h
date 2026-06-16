#pragma once

#include "Config.h"
#include "DownloadManager.h"
#include "StatsManager.h"
#include "Threading.h"

#include <atomic>

using namespace std;

class Dashboard {
public:
    Dashboard(const Config& cfg, StatsManager& stats, DownloadManager& dlMgr);

    void start();
    void stop();
    void printOnce() const;

private:
    void loop();

    const Config& cfg_;
    StatsManager& stats_;
    DownloadManager& dlMgr_;
    atomic<bool> running_{false};
    Thread th_;
};
