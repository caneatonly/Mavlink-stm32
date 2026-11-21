cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32-mavlink)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    App/common/ring_buffer.c
    App/vendor_protocol/vendor_protocol.c
    App/mavlink_handler/mavlink_handler.c
    App/status_reporter/status_reporter.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    App/common
    App/vendor_protocol
    App/mavlink_handler
    App/status_reporter
)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PRIVATE ThirdParty/mavlink)

# Add project symbols (macros)
option(ENABLE_POINTING_AUTO_RESUME "Enable auto-resume after the TELEM2 order is verified" OFF)
option(ENABLE_RANGE_REPORT "Send ID 86 position setpoints from tracked range results" OFF)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    ENABLE_POINTING_AUTO_RESUME=$<BOOL:${ENABLE_POINTING_AUTO_RESUME}>
    ENABLE_RANGE_REPORT=$<BOOL:${ENABLE_RANGE_REPORT}>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    m

    # Add user defined libraries
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/tz130tm-native.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${CMAKE_CURRENT_BINARY_DIR}/tz130tm-native.bin
    VERBATIM
)
