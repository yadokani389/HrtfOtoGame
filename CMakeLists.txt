cmake_minimum_required(VERSION 3.12)
project(HrtfOtoGame CXX)

if (NOT CMAKE_CONFIGURATION_TYPES AND 
    NOT CMAKE_NO_BUILD_TYPE AND
    NOT CMAKE_BUILD_TYPE AND
    CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "[!] Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(HrtfOtoGame
  main.cpp
  )

add_subdirectory(third_party/ooura-fft-cpp)
find_package(Siv3D)
target_link_libraries(HrtfOtoGame PUBLIC Siv3D::Siv3D fftsg)

target_compile_features(HrtfOtoGame PRIVATE cxx_std_20)

if(BUILD_TESTING)
enable_testing()
add_test(
  NAME Test
  COMMAND HrtfOtoGame
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endif()

