#pragma once

#include "Threading.h"

#include <atomic>

using namespace std;

class StatsManager {
public:
    void addDownload(int bytes);
    void addUpload(int bytes);
    double downloadSpeedMbps() const;
    double uploadSpeedMbps() const;
    void tick();

private:
    atomic<long long> dlTotal_{0};
    atomic<long long> ulTotal_{0};
    mutable Mutex mu_;
    long long dlLastSec_ = 0;
    long long ulLastSec_ = 0;
    double dlSpeed_ = 0;
    double ulSpeed_ = 0;
};
