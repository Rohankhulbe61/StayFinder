# CMakeLists.txt - the build instructions. CMake reads this file and sets up the compiler.
# You do not need to edit this file to run the project.
cmake_minimum_required(VERSION 3.16)
project(StayFinder LANGUAGES C CXX)

# Use C++17 for our code.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimised build if you do not choose one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Needed by the web server library. No other packages are required:
# SQLite is included in this folder as sqlite3.c and sqlite3.h.
find_package(Threads REQUIRED)

# Full paths the program needs. They are based on where this file is,
# so the program can find the website and the database from any folder.
get_filename_component(FRONTEND_PATH "${CMAKE_SOURCE_DIR}/../frontend" ABSOLUTE)
set(DATABASE_FILE "${CMAKE_SOURCE_DIR}/stayfinder.db")

add_executable(stayfinder
    main.cpp
    database.cpp
    sqlite_booking_service.cpp
    sqlite3.c          # the SQLite database engine (one big C file)
)

# Pass the two paths into the C++ code as FRONTEND_DIR and DATABASE_PATH.
target_compile_definitions(stayfinder PRIVATE
    FRONTEND_DIR="${FRONTEND_PATH}"
    DATABASE_PATH="${DATABASE_FILE}"
)

target_link_libraries(stayfinder PRIVATE Threads::Threads)

# On Windows, the web server library needs these system libraries.
if(WIN32)
    target_link_libraries(stayfinder PRIVATE ws2_32 mswsock)
endif()

# Extra warnings for GCC/Clang. Visual Studio (MSVC) has its own warning settings.
if(NOT MSVC)
    target_compile_options(stayfinder PRIVATE -Wall -Wextra)
    set_source_files_properties(sqlite3.c PROPERTIES COMPILE_OPTIONS "-w")  # keep SQLite quiet
endif()
