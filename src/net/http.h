// this file declares TileDownloader -> downloads the map tiles in the background.
// it is in a separate folder bcuz windows.h and raylib.h both define things like
// Rectangle and DrawText, so they can't be in the same .cpp file.
// it uses a thread so the window doesn't freeze while a tile is downloading.
#pragma once // this prevents multiple inclusions of the header file.

#include <condition_variable> // lets the thread sleep until there is work
#include <mutex> // lock so both threads don't use the lists at the same time
#include <string>
#include <thread>

#include "core/dynamic_array.h"

namespace net {

// one tile to download
struct DownloadJob {
    std::string key;  // "z/x/y"
    std::string url;  // https://tile.openstreetmap.org/z/x/y.png
    std::string path; // cache/z/x/y.png
};

// result of one job
struct DownloadResult {
    std::string key;
    bool ok = false;
};

class TileDownloader {
public:
    explicit TileDownloader(const std::string& userAgent); // starts the thread
    ~TileDownloader(); // stops the thread

    TileDownloader(const TileDownloader&) = delete;
    TileDownloader& operator=(const TileDownloader&) = delete;

    void enqueue(const DownloadJob& job); // add a job
    int pending() const; // jobs waiting + running
    bool popResult(DownloadResult& out); // get one finished job (false if none)

private:
    void workerLoop(); // the code the thread runs

    std::string userAgent_;
    mutable std::mutex mutex_; // mutable -> can be locked inside const functions
    std::condition_variable wake_; // wakes the thread when a job is added
    DynamicArray<DownloadJob> jobs_;
    DynamicArray<DownloadResult> results_;
    int running_ = 0;
    bool stop_ = false;
    std::thread worker_; // the background thread
};

// downloads one url into a file (blocking)
bool downloadToFile(const std::string& url, const std::string& path, const std::string& userAgent);

}
