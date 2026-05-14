cmake_minimum_required(VERSION 3.10)
project(dynamic_communication_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
find_package(vsomeip3 CONFIG REQUIRED)

add_library(dynamic_comm_lib
    lib/core/PatternRuntimeConfig.cpp
    lib/patterns/Request_Response/RequestResponse.cpp
    lib/patterns/Fire_Forget/FireForget.cpp
    lib/patterns/Event/Event.cpp
    lib/patterns/Field/Field.cpp
)

target_include_directories(dynamic_comm_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dynamic_comm_lib PUBLIC
    vsomeip3
    Threads::Threads
)

add_executable(main
    src/main.cpp
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(main PRIVATE
    dynamic_comm_lib
)

add_executable(wireshark_capture
    src/wireshark_capture.cpp
)

add_executable(dynamic_modular_test
    tests/dynamic_modular_test.cpp
)

target_include_directories(dynamic_modular_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(dynamic_modular_test PRIVATE
    dynamic_comm_lib
)
