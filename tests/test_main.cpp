// test_main.cpp - the only file with main() in the test program.
#include "test_framework.h"

void runDynamicArrayTests();   // Phase 2
void runMinHeapTests();   // Phase 3
void runHashTableTests();   // Phase 3
void runGraphTests();   // Phase 4
void runDijkstraTests();   // Phase 4
void runReliefTests();      // Phase 5
void runLoaderTests();      // Phase 6

int main() {
    runDynamicArrayTests();
    runMinHeapTests();
    runHashTableTests();
    runGraphTests();
    runDijkstraTests();
    runReliefTests();
    runLoaderTests();
    return test::testSummary();
}
