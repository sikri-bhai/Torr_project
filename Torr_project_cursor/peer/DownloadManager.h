#pragma once

#include "Config.h"
#include "FileAssembler.h"
#include "HashManager.h"
#include "Net.h"
#include "PieceManager.h"
#include "ResumeManager.h"
#include "Scheduler.h"
#include "StatsManager.h"
#include "Threading.h"
#include "TrackerClient.h"
#include "Types.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

using namespace std;

struct DownloadStatus {
    string fileName;
    int progressPct = 0;
    int piecesDone = 0;
    int piecesTotal = 0;
    int connectedPeers = 0;
    bool active = false;
};

class DownloadManager {
public:
    DownloadManager(const Config& cfg, TrackerClient& tracker, PieceManager& pieceMgr,
                    ResumeManager& resume, StatsManager& stats);

    bool startDownload(const string& fileName);
    void stop();
    DownloadStatus status() const;

private:
    void workerLoop(SwarmPeer peer);
    bool requestPiece(sock_t sock, int idx, vector<char>& data);
    void refreshAvailability();
    void syncState();
    void syncTracker();

    const Config& cfg_;
    TrackerClient& tracker_;
    PieceManager& pieceMgr_;
    ResumeManager& resume_;
    StatsManager& stats_;
    FileAssembler assembler_;

    unique_ptr<Scheduler> sched_;
    vector<string> hashes_;
    vector<SwarmPeer> peers_;
    vector<Thread> workers_;
    atomic<bool> running_{false};
    atomic<bool> dlActive_{false};
    mutable Mutex statusMu_;
    DownloadStatus status_;
    Thread syncTh_;
};
