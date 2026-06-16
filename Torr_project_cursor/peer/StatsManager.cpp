#include "StatsManager.h"

using namespace std;

void StatsManager::addDownload(int bytes) {
    dlTotal_ += bytes;
}

void StatsManager::addUpload(int bytes) {
    ulTotal_ += bytes;
}

void StatsManager::tick() {
    LockGuard lock(mu_);
    long long dl = dlTotal_.load();
    long long ul = ulTotal_.load();
    dlSpeed_ = (dl - dlLastSec_) / 1024.0 / 1024.0;
    ulSpeed_ = (ul - ulLastSec_) / 1024.0 / 1024.0;
    dlLastSec_ = dl;
    ulLastSec_ = ul;
}

double StatsManager::downloadSpeedMbps() const {
    LockGuard lock(mu_);
    return dlSpeed_;
}

double StatsManager::uploadSpeedMbps() const {
    LockGuard lock(mu_);
    return ulSpeed_;
}
