# MATAR Debug Flag Feature

## Overview
The MATAR build system now supports a debug flag that allows users to build the project with debug symbols and optimizations disabled, making it easier to debug applications.

## Usage

### Basic Usage
```bash
# Build with debug enabled
source scripts/build-matar.sh --debug=enabled

# Build with debug disabled (default)
source scripts/build-matar.sh --debug=disabled
```

### Combined with Other Options
```bash
# Build examples with debug enabled
source scripts/build-matar.sh --debug=enabled --execution=examples

# Build tests with debug enabled and OpenMP
source scripts/build-matar.sh --debug=enabled --execution=test --kokkos_build_type=openmp

# Build benchmarks with debug enabled
source scripts/build-matar.sh --debug=enabled --execution=benchmark
```

## How It Works

### Main Script (`build-matar.sh`)
- Added `--debug=<enabled|disabled>` flag with default value `disabled`
- The flag is validated and propagated to all relevant build scripts
- Debug flag status is displayed in the build options summary

### Build Scripts Modified
1. **`cmake_build_examples.sh`** - Sets `CMAKE_BUILD_TYPE=Debug` for example builds
2. **`cmake_build_test.sh`** - Sets `CMAKE_BUILD_TYPE=Debug` for test builds  
3. **`cmake_build_benchmark.sh`** - Sets `CMAKE_BUILD_TYPE=Debug` for benchmark builds
4. **`matar-install.sh`** - Sets `CMAKE_BUILD_TYPE=Debug` for MATAR library installation

### CMAKE Build Types
- **Debug Mode** (`--debug=enabled`): 
  - Sets `CMAKE_BUILD_TYPE=Debug`
  - Enables debug symbols (`-g`)
  - Disables optimizations (`-O0`)
  - Enables runtime checks
  
- **Release Mode** (`--debug=disabled`):
  - Sets `CMAKE_BUILD_TYPE=Release`
  - Enables optimizations (`-O3`)
  - Disables debug symbols by default
  - Optimized for performance

## Examples

### Debug a Specific Example
```bash
# Build examples with debug symbols
source scripts/build-matar.sh --debug=enabled --execution=examples --build_action=full-app

# Run your debugger on the built example
gdb ./build/examples/your_example
```

### Debug Tests
```bash
# Build tests with debug symbols
source scripts/build-matar.sh --debug=enabled --execution=test --build_action=full-app

# Run tests with debugging enabled
cd build/test && ./your_test
```

## Integration
The debug flag is fully integrated into the existing build system and works with all existing options:
- All `--kokkos_build_type` options (serial, openmp, cuda, etc.)
- All `--machine` options (linux, darwin, mac, etc.)
- All `--build_action` options (full-app, install-matar, etc.)
- All `--execution` options (examples, test, benchmark)

## Implementation Details
- The debug flag is consistently propagated through all build scripts
- Google Benchmark builds are also configured with the appropriate debug/release settings
- Error handling and validation are consistent with existing build system patterns
- Help documentation has been updated to include the new flag