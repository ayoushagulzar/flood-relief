# Flood Relief

**Disaster relief resource allocation and route planning for floods in Pakistan.**

Flood Relief models cities, shelters and supply depots as a road network. Affected areas send
requests for food, water, medicine and shelter. The system serves the most urgent requests first,
finds depots that have stock, plans the fastest open route with Dijkstra's algorithm, and
re-plans when roads are cut or reopened.

> Status: **in development** (v1.0 target: November 2026). Educational prototype using simulated data.

## Features (planned for v1.0)

- Road network of real Pakistani cities on a real map
- Priority queue of relief requests (urgency, people affected, waiting time)
- Inventory tracking across multiple depots
- Shortest routes with Dijkstra; roads can be blocked, slowed or reopened
- Deferred requests retried automatically when conditions change
- Statistics: fulfilled requests, unmet demand, route time, waiting time
- Command-line interface and raylib GUI

## Data structures

All core data structures are implemented from scratch.

| Structure | Used for | Key operations |
|---|---|---|
| Dynamic array | Storage everywhere | push: O(1) amortized |
| Graph (adjacency list) | Road network | neighbors: O(degree) |
| Binary min-heap | Request priority, Dijkstra | push/pop: O(log n) |
| Hash table | Request ids, city names | insert/find: O(1) average |
| Dijkstra | Fastest open route | O((V + E) log V) |

## Building

Requirements: a 64-bit C++17 compiler (MinGW-w64, GCC, Clang or MSVC), CMake 3.20+, Git.
raylib is downloaded automatically on the first configure.

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

Run:

```bash
build/bin/relief_cli
build/bin/relief_gui
ctest --test-dir build --output-on-failure
```

To build without the GUI (no download needed): add `-DFLOOD_BUILD_GUI=OFF` to the first command.

## Project structure

```
src/core/     data structures and algorithms (no raylib)
src/relief/   relief logic: requests, depots, allocation (no raylib)
src/gui/      raylib map and panels
src/net/      map tile downloading
app/          entry points for CLI and GUI
tests/        unit and integration tests
data/         scenario files
assets/       fonts and icons
tools/        small experiments and helper programs
docs/         proposal and planning documents
```

## Credits

- [raylib](https://www.raylib.com/) by Ramon Santamaria (zlib license)
- Map data © [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors
