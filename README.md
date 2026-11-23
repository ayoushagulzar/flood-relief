# Flood Relief: Disaster Relief Allocation and Route Planning for Pakistan

<p align="center">
  <strong>A flood relief planner in C++17 that decides who gets help first, from which depot, and by which road</strong><br>
  Data Structures Course Project · FAST-NUCES Karachi · Fall 2026
</p>

<p align="center">
  <img src="https://img.shields.io/badge/LANGUAGE-C%2B%2B17-3BA9F5?style=for-the-badge&labelColor=2A3142">
  <img src="https://img.shields.io/badge/COURSE-DATA%20STRUCTURES-7057E8?style=for-the-badge&labelColor=2A3142">
  <img src="https://img.shields.io/badge/GUI-RAYLIB%205.5-D63384?style=for-the-badge&labelColor=2A3142">
  <img src="https://img.shields.io/badge/TESTS-318%20PASSED-2E8B57?style=for-the-badge&labelColor=2A3142">
</p>


---

## About

Flood Relief is inspired by the **2022 monsoon floods in Pakistan**. During a flood the relief office gets many requests at once, supplies are kept in a few depots, and roads keep getting flooded or blocked. This program helps with that:

- it keeps all requests in a **priority queue (binary min-heap)** so the most urgent one is always served first, with **aging** so old requests are not forgotten
- it stores the road network as a **graph (adjacency list)** and uses **Dijkstra's algorithm** to find the fastest open route
- it takes supplies from the **nearest depots first** (greedy), and retries requests automatically when a road is reopened or new stock arrives
- it **explains every decision** in plain words

All data structures (**DynamicArray, MinHeap, HashTable, Graph, Dijkstra**) are written from scratch, no STL containers. The project has a **command-line interface** and a **raylib GUI** that shows everything on a real map of Pakistan.

The scenario file has **77 places, 97 roads, 9 depots and 28 requests** covering all provinces, Gilgit-Baltistan and AJK. The data is simulated, but the place names and coordinates are real.

---

## Features

| Feature | Details |
|---|---|
| **Priority queue with aging** | `base = urgency × 100 + people / 100 + 50 (if vulnerable)` and `key = base − 2 × arrival`. The key never changes, so the heap never needs rebuilding as time passes. |
| **Fastest routes** | Dijkstra with **lazy deletion** (no decrease-key needed). Blocked roads are skipped. |
| **Road conditions** | Roads can be clear, damaged (×1.5 time), flooded (×2.5 time) or blocked. Blocking always closes both directions. |
| **Greedy allocation** | Takes stock from the nearest depot first, and splits one request across several depots if needed. |
| **Automatic retry** | Requests that are partly served or can't be reached wait in a deferred list and are tried again after any road or stock change. |
| **"Why" explanation** | Every decision shows the route, the roads used, the skipped depots with reasons, and the priority calculation. |
| **Fast name search** | Place names and request ids (`R-07`) are found with my own **HashTable** (djb2, chaining, rehash at load factor 0.75). |
| **Clear file errors** | A broken scenario file gives the exact line, e.g. `pakistan.txt:115: unknown place 'Sukur' in [ROADS]`. |
| **CLI** | Coloured output, `queue` command that prints the heap array level by level and checks the heap order, and a `bench` command. |
| **GUI** | Real OpenStreetMap tiles (cached, downloaded on a background thread), zoom and pan, search, Close road and Add request tools, and five pages: Overview, Queue, Depots, Why, Events. |
| **Unit tests** | 318 checks on every data structure and the relief logic, with my own small test framework. |

---

## Data Structures

| Data structure | Used for | Main operations |
|---|---|---|
| `DynamicArray<T>` | storage everywhere (nodes, roads, requests, events, heap) | pushBack **O(1)** amortized (capacity doubles) |
| `MinHeap<T, Compare>` | request priority queue, Dijkstra | push / pop **O(log n)**, top **O(1)** |
| `HashTable` | place name → id, request id → index, map tile `"z/x/y"` → texture | insert / find **O(1)** average |
| `Graph` (adjacency list) | the road network | neighbours **O(degree)**, memory **O(V + E)** |
| Dijkstra | fastest open route from a place to all others | **O((V + E) log V)** |

**Benchmark:** the CLI `bench` command runs Dijkstra 100 times on a random graph with 2,000 places and 6,000 roads. One run takes about **0.55 ms** on my laptop (Release build).

---

## Project Structure

```text
flood-relief/
├── .github/
│   └── pull_request_template.md
├── app/
│   ├── cli_main.cpp          
│   └── gui_main.cpp          
├── src/
│   ├── core/                 
│   │   ├── dynamic_array.h
│   │   ├── min_heap.h
│   │   ├── hash_table.h / .cpp
│   │   ├── graph.h / .cpp
│   │   └── dijkstra.h / .cpp
│   ├── relief/              
│   │   ├── types.h / .cpp
│   │   ├── relief_manager.h / .cpp
│   │   └── scenario_loader.h / .cpp
│   ├── gui/                  
│   └── net/                  
├── tests/                   
├── data/
│   └── pakistan.txt        
├── assets/fonts/             
├── report/
│   ├── Flood_Relief_Report.pdf
│   └── screenshots/
├── tools/                    
└── CMakeLists.txt
```

The CLI and GUI only call `ReliefManager`. Nothing in `src/core` or `src/relief` includes raylib, so the same logic is used by both programs and the tests.

---

## How to Build and Run

**Requirements:** a C++17 compiler (I used **MSYS2 UCRT64 g++** on Windows), **CMake 3.20+** and **Git**. raylib 5.5 is downloaded automatically by CMake the first time.

```bash
git clone https://github.com/ayoushagulzar/flood-relief.git
cd flood-relief
cmake -S . -B build-release -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

Run:

```bash
./build-release/bin/relief_cli.exe      # command-line program
./build-release/bin/relief_gui.exe      # GUI (needs internet for the map tiles)
./build-release/bin/relief_tests.exe    # unit tests -> 318 passed, 0 failed
```

To build without the GUI (no raylib download), add `-DFLOOD_BUILD_GUI=OFF` to the first cmake command.

> **Windows Defender:** if linking fails with `ld returned 1` and no message, Defender is blocking the new .exe. Add the `build-release` folder to **Windows Security → Exclusions**.

### Demo (CLI)

| Command | What happens |
|---|---|
| `next` | R-01 Dadu Camp gets 1,200 water from Sukkur Depot via N-5, Dadu-Moro Bridge (3 h 30 min) |
| `why` | full explanation + priority key 630 |
| `block Jamshoro, Sehwan` | N-55 closed, later routes change |
| `all` | serves everything; R-17 Gilgit and R-24 Skardu are deferred (KKH is blocked), R-14 Sanghar is partial |
| `reopen Abbottabad, Gilgit` | the 3 deferred requests go back into the heap |
| `next` | Gilgit is served from Islamabad via N-35, KKH (12 h 45 min) |
| `queue`, `stats`, `bench` | heap view, totals and unmet demand, Dijkstra speed |

Type `help` for all commands. Place names with spaces are separated by commas.

### GUI keys

`N` next request · `A` serve all · `B` close/reopen road tool · `R` add request tool · `/` search · `Home` reset map · `Esc` cancel

---

## Known Limitations

This is a course project, not a real disaster management system:
- Allocation is **greedy** (nearest depot first). It is simple and easy to explain, but not always the best plan overall.
- The data is **simulated**. Travel times are estimates, deliveries are instant, and there are no trucks or capacities.
- Map tiles only download on **Windows** (WinINet). On other systems the map shows a simple Pakistan outline.
- Changes made in the program are not saved back to the scenario file.

---

## Project Report

The full report is in [report/Flood_Relief_Report.pdf](report/Flood_Relief_Report.pdf).

---

## Author

| Name | Roll No. |
|---|---|
| **Ayousha Gulzar** | 25K-3069 |

Instructor: **Miss Kinza Mushtaq**

---

## Credits

- T. H. Cormen et al., *Introduction to Algorithms*, 3rd ed., MIT Press
- M. A. Weiss, *Data Structures and Algorithm Analysis in C++*, 4th ed., Pearson
- [raylib](https://www.raylib.com) for the window and drawing
- Map data © [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors
- Poppins font by the Indian Type Foundry ([SIL Open Font License](assets/fonts/OFL.txt))

---

<p align="center">
  <i>Built as the Data Structures course project at FAST-NUCES Karachi, Fall 2026.</i>
</p>
