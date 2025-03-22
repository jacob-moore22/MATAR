#include <stdio.h>
#include <iostream>
#include <Kokkos_Core.hpp>
#include <matar.h>
#include "timer.hpp"

// Required for MATAR data structures
using namespace mtr; 

#define ARRAY_SIZE 1048576  // 1M elements

// Function to calculate theoretical FLOPS for stream triad
double calculate_flops(int size, double time_ms) {
    // For stream triad A = B + scalar * C:
    // Each element requires 2 operations (1 multiplication + 1 addition)
    double total_ops = static_cast<double>(size) * 2.0;
    double time_seconds = time_ms / 1000.0;
    return total_ops / time_seconds;
}

// main
int main(int argc, char* argv[])
{
    Kokkos::initialize(argc, argv);
    { // kokkos scope
    printf("Starting MATAR Stream Triad test \n");
    printf("Array size: %d elements\n", ARRAY_SIZE);

    // Create arrays on the device
    CArrayKokkos<double> A(ARRAY_SIZE);
    CArrayKokkos<double> B(ARRAY_SIZE);
    CArrayKokkos<double> C(ARRAY_SIZE);
    
    const double scalar = 3.0;

    // Initialize arrays (NOTE: This is on the device)
    B.set_values(2.0);
    C.set_values(1.0);
    A.set_values(0.0);

    // Create and start timer
    Timer timer;
    timer.start();

    // Perform stream triad: A = B + scalar * C
    FOR_ALL(i, 0, ARRAY_SIZE, {
        A(i) = B(i) + scalar * C(i);
    });
    // Add a fence to ensure all operations are completed
    kokkos::fence();

    // Stop timer and get execution time
    double time_ms = timer.stop();
    
    // Calculate and print performance metrics
    double flops = calculate_flops(ARRAY_SIZE, time_ms);
    printf("Execution time: %.2f ms\n", time_ms);
    printf("Performance: %.2f GFLOPS\n", flops / 1e9);

    // Calculate memory bandwidth
    size_t bytes_transferred = 3 * ARRAY_SIZE * sizeof(double);  // Read B,C and write A
    double bandwidth = (bytes_transferred / (time_ms / 1000.0)) / 1.0e9;  // GB/s
    printf("Memory Bandwidth: %.2f GB/s\n", bandwidth);

    } // end kokkos scope
    Kokkos::finalize();

    return 0;
}