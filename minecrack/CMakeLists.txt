cmake_minimum_required(VERSION 3.0)

add_executable(mincrack main.cpp sysapi.hpp sysapi.cpp)
target_link_libraries(mincrack fmt Gdi32 Gdiplus)

set_target_properties(mincrack PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY ../bin
    OUTPUT_NAME_Debug        ${OUTPUT_NAME}_d
)
    