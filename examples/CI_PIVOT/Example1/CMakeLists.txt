cmake_minimum_required(VERSION 3.1.3)

find_package(Matar REQUIRED)

if (KOKKOS)
  #find_package(Kokkos REQUIRED) #new
  
  add_executable(matmul matar_matmul.cpp)

  add_definitions(-DHAVE_KOKKOS=1)

  target_link_libraries(matmul ${LINKING_LIBRARIES})
endif()