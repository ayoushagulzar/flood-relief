// http.h - downloads map tiles in the background.
//
// Why a separate folder that NEVER includes raylib?
// windows.h (needed for WinINet) and raylib.h both define Rectangle,
// CloseWindow, ShowCursor, DrawText... Including both in one .cpp file is a
// compile error. So all Windows networking lives in http.cpp, and the GUI
// only sees this small header (plain C++ types, no windows.h, no raylib.h).
//
// Why a background thread?
// One tile takes 100-500 ms to download. Doing that inside the frame loop
// would freeze the window. The worker thread downloads; the GUI thread only
// enqueues jobs and collects results (cheap), at most a few per frame.
//
// On non-Windows builds the downloader reports every job as failed, so the
// GUI simply shows the map without tiles.
#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "core/dynamic_array.h"

namespace net {

struct DownloadJob {
    std::string key;   // "z/x/y", so the GUI knows which tile finished
    std::string url;   // https://tile.openstreetmap.org/z/x/y.png
    std::string path;  // cache/z/x/y.png
};

struct DownloadResult {
    std::string key;
    bool ok = false;
};

class TileDownloader {
public:
    explicit TileDownloader(const std::string& userAgent);
    ~TileDownloader();  // stops the worker thread and waits for it

    TileDownloader(const TileDownloader&) = delete;
    TileDownloader& operator=(const TileDownloader&) = delete;

    // GUI thread. O(1) amortized. Adds a job to the queue.
    void enqueue(const DownloadJob& job);
    // GUI thread. Jobs queued or running right now.
    int pending() const;
    // GUI thread. Takes one finished result; false if none is ready.
    bool popResult(DownloadResult& out);

private:
    void workerLoop();

    std::string userAgent_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    DynamicArray<DownloadJob> jobs_;        // protected by mutex_
    DynamicArray<DownloadResult> results_;  // protected by mutex_
    int running_ = 0;                       // protected by mutex_
    bool stop_ = false;                     // protected by mutex_
    std::thread worker_;
};

// Blocking download of url into path (folders are created). Used by the worker.
bool downloadToFile(const std::string& url, const std::string& path, const std::string& userAgent);

}  // namespace net
