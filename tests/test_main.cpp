// this file has the main() of the test program, it runs all the tests
#include "test_framework.h"

// these are written in the other test files
void runDynamicArrayTests();
void runMinHeapTests();
void runHashTableTests();
void runGraphTests();
void runDijkstraTests();
void runReliefTests();
void runLoaderTests();

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
