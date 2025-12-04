cmake_minimum_required(VERSION 3.16)

project(ChessEngineGUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -pedantic -O2)
endif()

set(SRC_FILES
    src/main.cpp
    src/board.cpp
    src/move.cpp
    src/search.cpp
    src/eval.cpp
    src/ui.cpp
    src/history.cpp
    src/notation.cpp
    src/uci.cpp
)

add_executable(chess ${SRC_FILES})
set_target_properties(chess PROPERTIES OUTPUT_NAME engine)

add_executable(chess_perft
    src/perft.cpp
    src/board.cpp
    src/move.cpp
    src/search.cpp
    src/eval.cpp
)

target_include_directories(chess PRIVATE src)
target_include_directories(chess_perft PRIVATE src)

find_package(SDL2 QUIET)
find_package(SDL2_image QUIET)

if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIR SDL.h
              PATH_SUFFIXES SDL2
              HINTS /usr/local/include /opt/homebrew/include /usr/include)
    find_library(SDL2_LIBRARY NAMES SDL2
                 HINTS /usr/local/lib /opt/homebrew/lib /usr/lib)
    if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
        set(SDL2_FOUND TRUE)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
        set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    endif()
endif()

if(NOT SDL2_IMAGE_FOUND)
    find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
              PATH_SUFFIXES SDL2
              HINTS /usr/local/include /opt/homebrew/include /usr/include)
    find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image
                 HINTS /usr/local/lib /opt/homebrew/lib /usr/lib)
    if(SDL2_IMAGE_INCLUDE_DIR AND SDL2_IMAGE_LIBRARY)
        set(SDL2_IMAGE_FOUND TRUE)
        set(SDL2_IMAGE_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
        set(SDL2_IMAGE_LIBRARIES ${SDL2_IMAGE_LIBRARY})
    endif()
endif()

if(SDL2_FOUND AND SDL2_IMAGE_FOUND)
    if(TARGET SDL2::SDL2)
        target_link_libraries(chess PRIVATE SDL2::SDL2 SDL2_image::SDL2_image)
    else()
        target_include_directories(chess PRIVATE ${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS})
        target_link_libraries(chess PRIVATE ${SDL2_LIBRARIES} ${SDL2_IMAGE_LIBRARIES})
    endif()
else()
    message(STATUS "SDL2 / SDL2_image not found; building engine without GUI linkage. Install them to enable SDL2 GUI.")
endif()
