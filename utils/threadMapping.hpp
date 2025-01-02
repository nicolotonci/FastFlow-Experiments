#include <iostream>
#include <pthread.h>
#include <string.h>

void pinThreadToCore(int core_id) {
    // Get the thread ID
    pthread_t thread = pthread_self();

    // Create a CPU set
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);          // Clear the CPU set
    CPU_SET(core_id, &cpuset);  // Add the desired core to the set

    // Set the thread's affinity
    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "Error setting thread affinity: " << strerror(result) << std::endl;
    } else {
        std::cout << "Thread pinned to core " << core_id << std::endl;
    }
}