// small test program i made at the start: can we download one map tile with WinINet?
// tile 5/22/13 = zoom 5, column 22, row 13 (south Pakistan)
#include <fstream>
#include <iostream>

#include <windows.h> // must be before wininet.h
#include <wininet.h>

int main() {
    const char* url = "https://tile.openstreetmap.org/5/22/13.png";
    const char* outFile = "tile.png";

    // 1. open internet session. OSM wants a User-Agent with the app name
    HINTERNET session = InternetOpenA("FloodRelief/0.1 (student project)",
                                      INTERNET_OPEN_TYPE_PRECONFIG,
                                      nullptr, nullptr, 0);
    if (!session) {
        std::cerr << "InternetOpenA failed, error " << GetLastError() << "\n";
        return 1;
    }

    // 2. open the url (sends the GET request)
    HINTERNET request = InternetOpenUrlA(session, url, nullptr, 0,
                                         INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!request) {
        std::cerr << "InternetOpenUrlA failed, error " << GetLastError() << "\n";
        InternetCloseHandle(session);
        return 1;
    }

    // 3. check status code, 200 = OK
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

    // 4. read 4 KB at a time and write to tile.png (binary bcuz png is not text)
    std::ofstream out(outFile, std::ios::binary);
    char buffer[4096];
    DWORD bytesRead = 0;
    unsigned long total = 0;
    while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        out.write(buffer, bytesRead);
        total += bytesRead;
    }

    // 5. close handles
    InternetCloseHandle(request);
    InternetCloseHandle(session);

    std::cout << "Downloaded " << total << " bytes to " << outFile << "\n";
    return total > 0 ? 0 : 1;
}
