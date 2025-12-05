cmake_minimum_required(VERSION 3.20)
project(grid_watcher VERSION 3.0.0 LANGUAGES CXX)

# ============================================================================
# Build Options
# ============================================================================
option(ENABLE_LTO "Enable Link-Time Optimization" ON)
option(ENABLE_NATIVE "Enable -march=native optimization" ON)
option(ENABLE_SIMD "Enable SIMD optimizations" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" ON)
# Kita default ON, tapi nanti dicek di bawah bisa jalan apa enggak
option(ENABLE_CAPTURE "Enable packet capture support" ON)

# ============================================================================
# C++ Standard
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

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(ARCH_X86_64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    set(ARCH_ARM64 TRUE)
endif()

# ============================================================================
# Platform Configuration (Windows & Npcap) - THE ZUU APPROVED WAY
# ============================================================================
set(PCAP_FOUND FALSE) # Default status

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    
    # 1. Setup Path SDK Npcap yang BENAR (Sesuai punya Rara)
    if(ENABLE_CAPTURE)
        set(NPCAP_SDK_PATH "D:/dev-tools/npcap-sdk-1.15")
        
        # Cek dulu folder SDK-nya beneran ada gak
        if(EXISTS "${NPCAP_SDK_PATH}/Include/pcap.h")
            message(STATUS "✅ Found Npcap SDK at: ${NPCAP_SDK_PATH}")
            
            include_directories(${NPCAP_SDK_PATH}/Include)
            link_directories(${NPCAP_SDK_PATH}/Lib/x64)
            
            # Link library yang dibutuhkan (JANGAN DITIMPA-TIMPA LAGI!)
            # ws2_32 = Sockets
            # psapi  = Process Status API (sering butuh di windows)
            # wpcap  = Libpcap implementation
            # packet = Driver interface
            set(PLATFORM_LIBS ws2_32 psapi wpcap packet)
            
            add_definitions(-DHAVE_LIBPCAP)
            add_definitions(-DWPCAP)
            set(PCAP_FOUND TRUE)
        else()
            message(WARNING "❌ Npcap SDK not found at ${NPCAP_SDK_PATH}. Capture disabled.")
            set(PLATFORM_LIBS ws2_32 psapi) # Fallback tanpa Npcap
        endif()
    else()
        set(PLATFORM_LIBS ws2_32 psapi)
    endif()

elseif(UNIX)
    set(PLATFORM_LIBS pthread)
    # Di Linux biasanya pakai find_package(PkgConfig) buat libpcap, 
    # tapi kita fokus Windows dulu buat Rara.
endif()

# ============================================================================
# Compiler Flags
# ============================================================================
if(COMPILER_IS_GCC_OR_CLANG)
    set(BASE_CXX_FLAGS
        -Wall -Wextra -Wpedantic
        -Wno-c++98-compat -Wno-c++98-compat-pedantic
        -Wno-sign-conversion -Wno-c++11-narrowing -Wno-padded
    )
    
    set(RELEASE_CXX_FLAGS
        -O3 -ffast-math -funroll-loops -finline-functions
        -fomit-frame-pointer -fstrict-aliasing
        -fvisibility=hidden -ftree-vectorize -fno-rtti
        -D_CRT_SECURE_NO_WARNINGS
    )
    
    if(ENABLE_NATIVE)
        list(APPEND RELEASE_CXX_FLAGS -march=native -mtune=native)
    endif()
    
    if(ENABLE_SIMD AND ARCH_X86_64)
        list(APPEND RELEASE_CXX_FLAGS -mavx2 -mfma -mbmi -mbmi2)
    endif()
    
    if(ENABLE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
        list(APPEND RELEASE_CXX_FLAGS -flto)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
    endif()
endif()

# ============================================================================
# Include Directories
# ============================================================================
include_directories(${CMAKE_SOURCE_DIR}/include)

# ============================================================================
# Source Files Headers
# ============================================================================
# ... (Header definitions sama seperti sebelumnya, aman) ...
set(ALL_HEADERS
    include/grid_watcher/core/ipv4.hpp
    include/grid_watcher/core/socket_address.hpp
    include/grid_watcher/core/socket_base.hpp
    include/grid_watcher/core/tcp_socket.hpp
    include/grid_watcher/meta/arithmetic.hpp
    include/grid_watcher/utils/convert.hpp
    include/grid_watcher/utils/clamp.hpp
    include/grid_watcher/utils/subnet.hpp
    include/grid_watcher/scada/types.hpp
    include/grid_watcher/scada/config.hpp
    include/grid_watcher/scada/modbus_parser.hpp
    include/grid_watcher/scada/behavioral_analyzer.hpp
    include/grid_watcher/scada/mitigation_engine.hpp
    include/grid_watcher/performance/lock_free.hpp
    include/grid_watcher/performance/fast_hash.hpp
    include/grid_watcher/performance/bloom_filter.hpp
    include/grid_watcher/performance/cache_utils.hpp
    include/grid_watcher/capture/logger.hpp
    include/grid_watcher/capture/statistics.hpp
    include/grid_watcher/capture/metrics.hpp
    include/grid_watcher/processing/packet_processor.hpp
    include/grid_watcher/web/web_server.hpp
    include/grid_watcher/grid_watcher.hpp
)

if(PCAP_FOUND)
    list(APPEND ALL_HEADERS include/grid_watcher/capture/packet_capture.hpp)
endif()

# ============================================================================
# Main Executables
# ============================================================================

# 1. Demo Application
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

# 2. CLI Application
# CATATAN ZUU: Rara, pastikan file src/grid_watcher.cpp ATAU src/cli_main.cpp
# beneran ada. Di file list kamu adanya 'src/cli_main.cpp', jadi aku pakai itu ya.
if(PCAP_FOUND)
    message(STATUS "🚀 Building CLI with Full Packet Capture Support")
    add_executable(grid_watcher
        src/grid_watcher.cpp    # <-- Pastikan nama file ini benar!
        ${ALL_HEADERS}
    )
else()
    message(WARNING "⚠️ Building CLI without packet capture (Monitoring Disabled)")
    add_executable(grid_watcher
        src/cli_main.cpp
        ${ALL_HEADERS}
    )
endif()

target_compile_options(grid_watcher PRIVATE
    ${BASE_CXX_FLAGS}
    $<$<CONFIG:Release>:${RELEASE_CXX_FLAGS}>
    $<$<CONFIG:Debug>:-g -O0>
)

target_link_libraries(grid_watcher PRIVATE ${PLATFORM_LIBS})

# 3. Benchmarks
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
# Final Summary
# ============================================================================
message(STATUS "════════════════════════════════════════════════════════")
message(STATUS "Grid-Watcher Status (Zuu's Check)")
message(STATUS "════════════════════════════════════════════════════════")
message(STATUS "Capture Support:       ${PCAP_FOUND}")
if(PCAP_FOUND)
    message(STATUS "Npcap Path:            ${NPCAP_SDK_PATH}")
else()
    message(STATUS "Status:                BLIND MODE (No Capture)")
endif()
message(STATUS "════════════════════════════════════════════════════════")