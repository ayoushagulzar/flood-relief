// Phase 1 go/no-go test: can we download one OpenStreetMap tile with WinINet?
// Tile 5/22/13 = zoom 5, column 22, row 13 -> covers southern Pakistan.

#include <fstream>
#include <iostream>

#include <windows.h> // must come before wininet.h (wininet uses its types)
#include <wininet.h>

int main() {
    const char* url = "https://tile.openstreetmap.org/5/22/13.png"; // where to download from
    const char* outFile = "tile.png";                               // output file name

    // 1. Open an internet "session". OSM requires a User-Agent that says who we are.
    //    The A at the end of InternetOpenA means it takes normal char strings.
    HINTERNET session = InternetOpenA("FloodRelief/0.1 (student project)",
                                      INTERNET_OPEN_TYPE_PRECONFIG, // use Windows' own internet settings (proxy etc.)
                                      nullptr, nullptr, 0);
    if (!session) {
        std::cerr << "InternetOpenA failed, error " << GetLastError() << "\n";
        return 1;
    }

    // 2. Open the URL (this sends the HTTP GET request).
    //    RELOAD = always download fresh; NO_CACHE_WRITE = don't store it in Windows' cache.
    HINTERNET request = InternetOpenUrlA(session, url, nullptr, 0,
                                         INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!request) {
        std::cerr << "InternetOpenUrlA failed, error " << GetLastError() << "\n";
        InternetCloseHandle(session); // clean up what we already opened
        return 1;
    }

    // 3. Check the HTTP status code: 200 means OK; anything else is a failure (e.g. 403 = refused).
    //    Without this, an error page could be saved as tile.png and look like success.
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status,
                       &statusSize, nullptr) &&
        status != 200) {
        std::cerr << "Server answered HTTP " << status << "\n";
        InternetCloseHandle(request);
        InternetCloseHandle(session);
        return 1;
    }

    // 4. Read the response in 4 KB chunks and write the raw bytes to tile.png.
    std::ofstream out(outFile, std::ios::binary); // binary: PNG is not text
    char buffer[4096];
    DWORD bytesRead = 0;
    unsigned long total = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        out.write(buffer, bytesRead); // write only the bytes we actually received
        total += bytesRead;
    }
    // bytesRead == 0 means the download is finished.

    // 5. Always close handles in reverse order of opening.
    InternetCloseHandle(request);
    InternetCloseHandle(session);

    std::cout << "Downloaded " << total << " bytes to " << outFile << "\n";
    return total > 0 ? 0 : 1; // 0 = success, 1 = failure
}
