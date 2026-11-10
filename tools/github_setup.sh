#!/usr/bin/env bash
# One-time GitHub setup: labels, milestones and the 10 phase issues.
# Requirements: GitHub CLI (gh), logged in with `gh auth login`.
# Run from the repo folder in Git Bash:  bash tools/github_setup.sh
set -e

echo "Creating labels..."
gh label create core          --color 0E7490 --description "Core data structures and algorithms" --force
gh label create dsa           --color 6D28D9 --description "Data structure work"                  --force
gh label create relief-logic  --color B45309 --description "Requests, depots, allocation"         --force
gh label create gui           --color 0891B2 --description "raylib interface"                     --force
gh label create test          --color 15803D --description "Tests"                                --force
gh label create docs          --color 64748B --description "Documentation"                        --force
gh label create bug           --color DC2626 --description "Something is broken"                  --force
gh label create enhancement   --color 2563EB --description "New feature or improvement"           --force

echo "Creating milestones..."
gh api "repos/{owner}/{repo}/milestones" -f title="v1.0 Course submission" -f due_on="2026-11-20T23:59:59Z" \
  -f description="Everything in the approved proposal plus the raylib GUI." >/dev/null
gh api "repos/{owner}/{repo}/milestones" -f title="v2.0 Semester break" \
  -f description="Inspector, smooth map, moving trucks and other upgrades." >/dev/null

M="v1.0 Course submission"
issue () { gh issue create --title "$1" --label "$2" --milestone "$M" --body "$3" >/dev/null; echo "  $1"; }

echo "Creating phase issues..."
issue "Phase 1: Project setup" "docs" "## Goal
Every tool works on my laptop.

## Tasks
- [ ] 64-bit g++, CMake and Git checked
- [ ] CMake builds relief_cli, relief_gui and relief_tests
- [ ] raylib window opens
- [ ] One OpenStreetMap tile downloaded with WinINet (tools/tile_test.cpp)

## Done when
All targets build from a clean build folder and the tile opens."

issue "Phase 2: DynamicArray<T>" "dsa,core" "## Goal
Generic resizable array used by every other structure.

## Tasks
- [ ] Constructor, destructor, copy constructor, operator=
- [ ] pushBack / popBack with capacity doubling
- [ ] operator[], at(), insertAt, removeAt, clear
- [ ] CHECK macro and tests

## Done when
All tests pass and there are no leaks."

issue "Phase 3: Graph + MinHeap" "dsa,core" "## Goal
Road network and priority queue.

## Tasks
- [ ] Node and Edge types (baseTime, condition, type, blocked)
- [ ] Graph: addNode, addRoad, block/reopen/setCondition (both directions), neighbors
- [ ] MinHeap<T, Compare>: push, pop, top, size, isEmpty
- [ ] Tests

## Done when
Heap sorts 1,000 random values and graph tests pass."

issue "Phase 4: Dijkstra + HashTable" "dsa,core" "## Goal
Fastest open routes and O(1) lookups.

## Tasks
- [ ] Dijkstra with start/step/isDone/run, distanceTo, pathTo
- [ ] Skip blocked roads, weight = baseTime x condition
- [ ] HashTable with chaining and rehash at load factor 0.75
- [ ] Tests (detour, unreachable, collisions, rehash)

## Done when
All tests pass, Dijkstra works step by step."

issue "Phase 5: Relief manager" "relief-logic" "## Goal
The brain: requests, depots and allocation.

## Tasks
- [ ] Request and Depot types, status enum
- [ ] Priority with aging (score = base - k x arrival)
- [ ] processNext with greedy multi-depot allocation
- [ ] Deferred requests retried on block/reopen/addStock
- [ ] Event log, clock, statistics
- [ ] Integration test

## Done when
Integration test covers every status."

issue "Phase 6: Loader + CLI" "relief-logic" "## Goal
Everything in the proposal works from the command line.

## Tasks
- [ ] Scenario file format and loader with line-number errors
- [ ] data/pakistan.txt and data/small_test.txt
- [ ] CLI menu
- [ ] Proposal test list incl. 2,000-node generated network
- [ ] Tag v0.9.0

## Done when
Every proposal feature works in the CLI."

issue "Phase 7: GUI map" "gui" "## Goal
Real Pakistan map with the road network on top.

## Tasks
- [ ] Decide basemap: OSM tiles or static image
- [ ] Layout, colours, font
- [ ] Web Mercator projection, Camera2D zoom and pan
- [ ] Basemap behind MapView::drawBasemap()
- [ ] Draw roads, cities, depots, labels

## Done when
Graph sits on the correct real cities; zoom and pan are smooth."

issue "Phase 8: GUI interaction" "gui" "## Goal
The app works end to end with the mouse.

## Tasks
- [ ] Click cities and roads (picking)
- [ ] Requests and Depots tabs
- [ ] Process next + route highlight
- [ ] Add-request form
- [ ] Event log and stats

## Done when
Load, process, block, reroute and reopen all work in the GUI."

issue "Phase 9: Polish" "gui" "## Goal
Looks and feels finished.

## Tasks
- [ ] Consistent colours, spacing, legend
- [ ] Inspector tab placeholder, tooltips
- [ ] GUI edge cases
- [ ] Demo story

## Done when
The demo runs twice without problems."

issue "Phase 10: Release v1.0" "docs" "## Goal
Submit.

## Tasks
- [ ] All tests and demo pass
- [ ] README with screenshots, architecture, complexity table
- [ ] Proposal v2
- [ ] Tag v1.0.0 and GitHub Release
- [ ] Viva rehearsal

## Done when
v1.0.0 released and submitted."

echo "Done. Open your repo's Issues and Milestones tabs to check."
