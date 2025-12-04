cmake_minimum_required(VERSION 3.20)
project(grid_watcher VERSION 3.0.0 LANGUAGES CXX)

# ============================================================================
# Build Options
# ============================================================================
option(ENABLE_LTO "Enable Link-Time Optimization" ON)
option(ENABLE_NATIVE "Enable -march=native optimization" ON)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)
option(ENABLE_PROFILING "Enable profiling support" OFF)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_WEB_DASHBOARD "Include web dashboard files" ON)
option(ENABLE_ASAN "Enable AddressSanitizer (debug)" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer (debug)" OFF)

# ============================================================================
# C++ Standard & Requirements
# ============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Output Directories
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

# ============================================================================
# Detect Compiler & Platform
# ============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(COMPILER_IS_GCC_OR_CLANG TRUE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(COMPILER_IS_MSVC TRUE)
endif()

# Detect CPU architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ARCH_X86_64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    set(ARCH_ARM64 TRUE)
endif()

# ============================================================================
# Compiler Flags - Base
# ============================================================================
if(COMPILER_IS_GCC_OR_CLANG)
    set(BASE_CXX_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wno-c++98-compat
        -Wno-c++98-compat-pedantic
        -Wno-sign-conversion
        -Wno-c++11-narrowing
        -Wno-padded  # We know about padding
    )
    
    # Release optimizations (AGGRESSIVE)
    set(RELEASE_CXX_FLAGS
        -O3                         # Maximum optimization
        -ffast-math                 # Fast floating-point
        -funroll-loops              # Aggressive loop unrolling
        -finline-functions          # Inline small functions
        -fomit-frame-pointer        # Remove frame pointer
        -fstrict-aliasing           # Strict aliasing rules
        -fvisibility=hidden         # Hide symbols by default
        -ftree-vectorize            # Auto-vectorization
        -fno-rtti                   # Disable RTTI (optional)
		-D_CRT_SECURE_NO_WARNINGS
    )
    
    # Native architecture optimization
    if(ENABLE_NATIVE)
        list(APPEND RELEASE_CXX_FLAGS
            -march=native           # Use CPU-specific instructions
            -mtune=native           # Tune for current CPU
        )
    endif()
    
    # SIMD optimizations
    if(ENABLE_SIMD AND ARCH_X86_64)
        list(APPEND RELEASE_CXX_FLAGS
            -mavx2                  # Enable AVX2
            -mfma                   # Enable FMA
            -mbmi                   # Enable BMI
            -mbmi2                  # Enable BMI2
        )
    endif()
    
    # Link-Time Optimization (LTO)
    if(ENABLE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        list(APPEND RELEASE_CXX_FLAGS -flto)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    endif()
    
    # Profiling support
    if(ENABLE_PROFILING)
        list(APPEND BASE_CXX_FLAGS -g -fno-omit-frame-pointer)
    endif()
    
    # AddressSanitizer (Debug)
    if(ENABLE_ASAN)
        list(APPEND BASE_CXX_FLAGS -fsanitize=address -fno-omit-frame-pointer)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    endif()
    
    # ThreadSanitizer (Debug)
    if(ENABLE_TSAN)
        list(APPEND BASE_CXX_FLAGS -fsanitize=thread)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    endif()
    
elseif(COMPILER_IS_MSVC)
    set(BASE_CXX_FLAGS
        /W4                         # Warning level 4
        /permissive-                # Strict standard conformance
        /Zc:__cplusplus             # Correct __cplusplus macro
    )
    
    set(RELEASE_CXX_FLAGS
        /O2                         # Maximum optimization
        /Ob2                        # Inline function expansion
        /Oi                         # Enable intrinsic functions
        /Ot                         # Favor fast code
        /GL                         # Whole program optimization
        /fp:fast                    # Fast floating-point
        /arch:AVX2                  # Enable AVX2
    )
    
    if(ENABLE_LTO)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    endif()
endif()

# ============================================================================
# Platform-Specific Settings
# ============================================================================
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
    set(PLATFORM_LIBS ws2_32)
elseif(UNIX)
    set(PLATFORM_LIBS pthread)
    
    # Find libpcap (optional, for packet capture)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(PCAP libpcap)
        if(PCAP_FOUND)
            add_definitions(-DHAVE_LIBPCAP)
            list(APPEND PLATFORM_LIBS ${PCAP_LIBRARIES})
            include_directories(${PCAP_INCLUDE_DIRS})
        endif()
    endif()
endif()

# ============================================================================
# Include Directories
# ============================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Source Files Organization
# ============================================================================

# Core networking headers
set(CORE_HEADERS
    include/grid_watcher/core/ipv4.hpp
    include/grid_watcher/core/socket_address.hpp
    include/grid_watcher/core/socket_base.hpp
    include/grid_watcher/core/tcp_socket.hpp
)

# Meta programming headers
set(META_HEADERS
    include/grid_watcher/meta/arithmetic.hpp
)

# Utility headers
set(UTILS_HEADERS
    include/grid_watcher/utils/convert.hpp
    include/grid_watcher/utils/clamp.hpp
    include/grid_watcher/utils/subnet.hpp
)

# SCADA-specific headers
set(SCADA_HEADERS
    include/grid_watcher/scada/types.hpp
    include/grid_watcher/scada/config.hpp
    include/grid_watcher/scada/modbus_parser.hpp
    include/grid_watcher/scada/behavioral_analyzer.hpp
    include/grid_watcher/scada/mitigation_engine.hpp
)

# Performance primitives
set(PERF_HEADERS
    include/grid_watcher/performance/lock_free.hpp
    include/grid_watcher/performance/fast_hash.hpp
    include/grid_watcher/performance/bloom_filter.hpp
    include/grid_watcher/performance/cache_utils.hpp
)

# Monitoring headers
set(MONITOR_HEADERS
    include/grid_watcher/monitor/logger.hpp
    include/grid_watcher/monitor/statistics.hpp
    include/grid_watcher/monitor/metrics.hpp
)

# Processing headers (NEW)
set(PROCESSING_HEADERS
    include/grid_watcher/processing/packet_processor.hpp
)

# Web API headers (NEW)
set(WEB_HEADERS
    include/grid_watcher/web/web_server.hpp
)

# Main engine
set(ENGINE_HEADERS
    include/grid_watcher/grid_watcher.hpp
)

# All headers
set(ALL_HEADERS
    ${CORE_HEADERS}
    ${META_HEADERS}
    ${UTILS_HEADERS}
    ${SCADA_HEADERS}
    ${PERF_HEADERS}
    ${MONITOR_HEADERS}
    ${PROCESSING_HEADERS}
    ${WEB_HEADERS}
    ${ENGINE_HEADERS}
)

# ============================================================================
# Main Executable (Demo)
# ============================================================================
add_executable(grid_watcher_demo
    src/demo.cpp
    ${ALL_HEADERS}
)

target_compile_options(grid_watcher_demo PRIVATE
    ${BASE_CXX_FLAGS}
    $<$<CONFIG:Release>:${RELEASE_CXX_FLAGS}>
    $<$<CONFIG:Debug>:-g -O0>
)

target_link_libraries(grid_watcher_demo PRIVATE ${PLATFORM_LIBS})

# ============================================================================
# Production CLI Executable (NEW)
# ============================================================================
add_executable(grid_watcher
    src/cli_main.cpp
    ${ALL_HEADERS}
)

target_compile_options(grid_watcher PRIVATE
    ${BASE_CXX_FLAGS}
    $<$<CONFIG:Release>:${RELEASE_CXX_FLAGS}>
    $<$<CONFIG:Debug>:-g -O0>
)

target_link_libraries(grid_watcher PRIVATE ${PLATFORM_LIBS})

# ============================================================================
# Benchmarks
# ============================================================================
if(BUILD_BENCHMARKS)
    add_executable(grid_watcher_benchmark
        src/benchmark.cpp
        ${ALL_HEADERS}
    )
    
    target_compile_options(grid_watcher_benchmark PRIVATE
        ${BASE_CXX_FLAGS}
        ${RELEASE_CXX_FLAGS}
    )
    
    target_link_libraries(grid_watcher_benchmark PRIVATE ${PLATFORM_LIBS})
endif()

# ============================================================================
# Unit Tests (TODO - requires test framework)
# ============================================================================
if(BUILD_TESTS)
    # find_package(GTest REQUIRED)
    # add_executable(grid_watcher_tests
    #     tests/test_main.cpp
    #     tests/test_ipv4.cpp
    #     tests/test_modbus_parser.cpp
    #     tests/test_behavioral_analyzer.cpp
    #     ${ALL_HEADERS}
    # )
    # target_link_libraries(grid_watcher_tests PRIVATE ${PLATFORM_LIBS} GTest::GTest)
    # enable_testing()
    # add_test(NAME grid_watcher_tests COMMAND grid_watcher_tests)
endif()

# ============================================================================
# Custom Targets - Performance Analysis
# ============================================================================

# Run benchmarks
add_custom_target(benchmark
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_benchmark
    DEPENDS grid_watcher_benchmark
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running performance benchmarks"
)

# Performance analysis with perf (Linux only)
if(UNIX AND NOT APPLE)
    add_custom_target(perf
        COMMAND perf record -g ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_benchmark
        COMMAND perf report
        DEPENDS grid_watcher_benchmark
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Running performance analysis with perf"
    )
    
    add_custom_target(perf_stat
        COMMAND perf stat -e cache-misses,cache-references,instructions,cycles,branches,branch-misses
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_benchmark
        DEPENDS grid_watcher_benchmark
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Collecting performance statistics"
    )
    
    add_custom_target(perf_annotate
        COMMAND perf record -g ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_benchmark
        COMMAND perf annotate --stdio
        DEPENDS grid_watcher_benchmark
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        COMMENT "Annotated performance analysis"
    )
endif()

# Valgrind memory check
add_custom_target(memcheck
    COMMAND valgrind --leak-check=full --show-leak-kinds=all
            --track-origins=yes --verbose
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_demo
    DEPENDS grid_watcher_demo
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running memory leak detection"
)

# Callgrind profiling
add_custom_target(callgrind
    COMMAND valgrind --tool=callgrind --dump-instr=yes --collect-jumps=yes
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_demo
    DEPENDS grid_watcher_demo
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running Callgrind profiler"
)

# Cachegrind profiling
add_custom_target(cachegrind
    COMMAND valgrind --tool=cachegrind --branch-sim=yes
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/grid_watcher_demo
    DEPENDS grid_watcher_demo
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMENT "Running Cachegrind profiler"
)

# ============================================================================
# Installation
# ============================================================================
install(TARGETS grid_watcher grid_watcher_demo
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Install configuration file
install(FILES config/grid_watcher.conf.example
    DESTINATION /etc/grid-watcher
    RENAME grid_watcher.conf
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

# Install systemd service file (Linux only)
if(UNIX AND NOT APPLE)
    install(FILES config/grid-watcher.service
        DESTINATION /lib/systemd/system
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
endif()

# Install web dashboard
if(BUILD_WEB_DASHBOARD)
    install(FILES web/dashboard.html
        DESTINATION share/grid-watcher/web
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
endif()

# ============================================================================
# CPack Configuration (Package Generation)
# ============================================================================
set(CPACK_PACKAGE_NAME "grid-watcher")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Grid-Watcher Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ultra-Fast SCADA Security Monitor")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# DEB package
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Your Name <your.email@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.27), libstdc++6 (>= 10)")

# RPM package
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")

include(CPack)

# ============================================================================
# Print Configuration Summary
# ============================================================================
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "Grid-Watcher v${PROJECT_VERSION} Build Configuration")
message(STATUS "═══════════════════════════════════════════════════════════")
message(STATUS "Build Type:            ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler:              ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard:          C++${CMAKE_CXX_STANDARD}")
message(STATUS "System:                ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "Optimizations:")
message(STATUS "  LTO:                 ${ENABLE_LTO}")
message(STATUS "  Native:              ${ENABLE_NATIVE}")
message(STATUS "  SIMD:                ${ENABLE_SIMD}")
message(STATUS "  Profiling:           ${ENABLE_PROFILING}")
message(STATUS "")
message(STATUS "Build Targets:")
message(STATUS "  Demo:                ON")
message(STATUS "  CLI:                 ON")
message(STATUS "  Benchmarks:          ${BUILD_BENCHMARKS}")
message(STATUS "  Tests:               ${BUILD_TESTS}")
message(STATUS "  Web Dashboard:       ${BUILD_WEB_DASHBOARD}")
message(STATUS "")
message(STATUS "Sanitizers:")
message(STATUS "  AddressSanitizer:    ${ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer:     ${ENABLE_TSAN}")
message(STATUS "")
message(STATUS "Install Prefix:        ${CMAKE_INSTALL_PREFIX}")
message(STATUS "═══════════════════════════════════════════════════════════")

# ============================================================================
# Build Instructions
# ============================================================================
message(STATUS "")
message(STATUS "Quick Build:")
message(STATUS "  mkdir build && cd build")
message(STATUS "  cmake -DCMAKE_BUILD_TYPE=Release ..")
message(STATUS "  cmake --build . -j$(nproc)")
message(STATUS "")
message(STATUS "Performance Analysis:")
message(STATUS "  make benchmark      # Run benchmarks")
message(STATUS "  make perf          # Profile with perf")
message(STATUS "  make perf_stat     # Collect statistics")
message(STATUS "  make callgrind     # Callgrind profiling")
message(STATUS "  make memcheck      # Memory leak detection")
message(STATUS "")
message(STATUS "Installation:")
message(STATUS "  sudo make install")
message(STATUS "═══════════════════════════════════════════════════════════")