// this file has the code of TileDownloader (WinINet + a background thread). no raylib here
#include "net/http.h"

#include <cstdio>
#include <filesystem> // to make folders and rename files
#include <system_error>

// windows internet library (only on windows)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wininet.h>
#endif

namespace net {

// constructor starts the thread right away
TileDownloader::TileDownloader(const std::string& userAgent) : userAgent_(userAgent) {
    worker_ = std::thread(&TileDownloader::workerLoop, this);
}

// tell the thread to stop, wake it up and wait for it (join)
TileDownloader::~TileDownloader() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

// lock_guard locks the mutex and unlocks it automatically at the end of { }
void TileDownloader::enqueue(const DownloadJob& job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.pushBack(job);
    }
    wake_.notify_one();
}

int TileDownloader::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(jobs_.size()) + running_;
}

// oldest result first
bool TileDownloader::popResult(DownloadResult& out) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (results_.isEmpty()) return false;
    out = results_[0];
    results_.removeAt(0);
    return true;
}

// thread loop: wait for a job -> download -> save result -> repeat
void TileDownloader::workerLoop() {
    for (;;) {
        DownloadJob job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock, [this] { return stop_ || !jobs_.isEmpty(); });
            if (stop_) return;
            job = jobs_[0];
            jobs_.removeAt(0);
            ++running_;
        }
        bool ok = downloadToFile(job.url, job.path, userAgent_); // slow part is done without the lock
        {
            std::lock_guard<std::mutex> lock(mutex_);
            --running_;
            DownloadResult r;
            r.key = job.key;
            r.ok = ok;
            results_.pushBack(r);
        }
    }
}

#ifdef _WIN32
bool downloadToFile(const std::string& url, const std::string& path, const std::string& userAgent) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec); // make cache/z/x folder

    HINTERNET net = InternetOpenA(userAgent.c_str(), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!net) return false;
    DWORD timeoutMs = 8000; // 8 seconds max
    InternetSetOptionA(net, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
    InternetSetOptionA(net, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

    HINTERNET file = InternetOpenUrlA(net, url.c_str(), nullptr, 0,
                                      INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_UI | INTERNET_FLAG_RELOAD, 0);
    if (!file) {
        InternetCloseHandle(net);
        return false;
    }

    DWORD status = 0, len = sizeof(status);
    HttpQueryInfoA(file, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &len, nullptr);
    bool ok = (status == 200); // only 200 = OK

    std::string tmp = path + ".part"; // write to .part first so half files don't stay in the cache
    FILE* out = ok ? std::fopen(tmp.c_str(), "wb") : nullptr;
    if (!out) ok = false;

    // read 8 KB at a time
    char buffer[8192];
    DWORD got = 0;
    long total = 0;
    while (ok && InternetReadFile(file, buffer, sizeof(buffer), &got) && got > 0) {
        if (std::fwrite(buffer, 1, got, out) != got) ok = false;
        total += static_cast<long>(got);
    }
    if (out) std::fclose(out);
    InternetCloseHandle(file);
    InternetCloseHandle(net);

    // worked -> rename .part to .png, else delete .part
    if (ok && total > 0) {
        std::filesystem::rename(tmp, path, ec);
        if (!ec) return true;
    }
    std::filesystem::remove(tmp, ec);
    return false;
}
#else
bool downloadToFile(const std::string&, const std::string&, const std::string&) {
    return false;
}
#endif

}
