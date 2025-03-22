#include <stdio.h>
#include <iostream>
#include <Kokkos_Core.hpp>
#include <matar.h>
#include "timer.hpp"

// Required for MATAR data structures
using namespace mtr; 

#define MATRIX_SIZE 1024

// Function to calculate theoretical FLOPS
double calculate_flops(int size, double time_ms) {
    // For matrix multiplication C = A * B:
    // Each element C(i,j) requires 2*size operations (size multiplications + size-1 additions)
    // Total operations = size * size * (2*size)
    double total_ops = static_cast<double>(size) * size * (2.0 * size);
    double time_seconds = time_ms / 1000.0;
    return total_ops / time_seconds;
}

// main
int main(int argc, char* argv[])
{
    Kokkos::initialize(argc, argv);
    { // kokkos scope
    printf("Starting MATAR Matrix Multiplication test \n");
    printf("Matrix size: %d x %d\n", MATRIX_SIZE, MATRIX_SIZE);

    // Create arrays on the device, where the device is either the CPU or GPU depending on how it is compiled
    CArrayKokkos<int> A(MATRIX_SIZE, MATRIX_SIZE);
    CArrayKokkos<int> B(MATRIX_SIZE, MATRIX_SIZE);
    CArrayKokkos<int> C(MATRIX_SIZE, MATRIX_SIZE);

    // Initialize arrays (NOTE: This is on the device)
    A.set_values(2);
    B.set_values(2);
    C.set_values(0);

    // Create and start timer
    Timer timer;
    timer.start();

    // Perform C = A * B
    FOR_ALL(i, 0, MATRIX_SIZE,
            j, 0, MATRIX_SIZE,
            k, 0, MATRIX_SIZE, {
        C(i,j) += A(i,k) * B(k,j);
    });

    // Stop timer and get execution time
    double time_ms = timer.stop();
    
    // Calculate and print performance metrics
    double flops = calculate_flops(MATRIX_SIZE, time_ms);
    printf("Execution time: %.2f ms\n", time_ms);
    printf("Performance: %.2f GFLOPS\n", flops / 1e9);

    } // end kokkos scope
    Kokkos::finalize();

    return 0;
}