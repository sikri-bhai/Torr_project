#pragma once

#include "Config.h"
#include "Dashboard.h"
#include "DownloadManager.h"
#include "HashManager.h"
#include "PieceManager.h"
#include "ResumeManager.h"
#include "StatsManager.h"
#include "Threading.h"
#include "TrackerClient.h"
#include "UploadServer.h"

#include <atomic>
#include <string>

using namespace std;

class Peer {
public:
    explicit Peer(Config cfg);

    bool start();
    void stop();
    bool shareFile(const string& path);
    bool downloadFile(const string& fileName);
    void waitForDownload();
    void printStatus() const;

private:
    Config cfg_;
    StatsManager stats_;
    PieceManager pieceMgr_;
    ResumeManager resume_;
    TrackerClient tracker_;
    UploadServer upload_;
    DownloadManager dlMgr_;
    Dashboard dashboard_;
    atomic<bool> started_{false};
    Thread dlTh_;
};
