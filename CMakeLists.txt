cmake_minimum_required(VERSION 3.25)
project(gbaemu_cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(3rd_party/ecnavdA-yoBemaG)

add_subdirectory(3rd_party/nativefiledialog-extended)

set(GBAEMU_SRC
        3rd_party/imgui/imgui.cpp
        3rd_party/imgui/imgui_demo.cpp
        3rd_party/imgui/imgui_draw.cpp
        3rd_party/imgui/imgui_tables.cpp
        3rd_party/imgui/imgui_widgets.cpp

        src/ppudebug.hpp
        src/ppudebug.cpp
        src/main.cpp)

if(WIN32)
    add_executable(gbaemu_cpp WIN32 ${GBAEMU_SRC})
else()
    add_executable(gbaemu_cpp ${GBAEMU_SRC})
endif()
target_include_directories(gbaemu_cpp PRIVATE
        3rd_party/sokol
        3rd_party/imgui
        3rd_party/imgui_club/imgui_memory_editor
        )
target_compile_definitions(gbaemu_cpp PRIVATE "BUILD_WITH_PPUDEBUG=1")
target_link_libraries(gbaemu_cpp ecnavda-yobemag nfd)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    if (MINGW)
        target_link_libraries(ecnavda-yobemag PRIVATE -lkernel32 -luser32 -lshell32 -lgdi32)
    endif ()
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    target_link_libraries(ecnavda-yobemag PRIVATE -framework Cocoa -framework QuartzCore -framework OpenGL)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_link_libraries(ecnavda-yobemag PRIVATE X11 Xi Xcursor GL dl pthread m)
endif ()
