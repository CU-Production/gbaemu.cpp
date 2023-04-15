cmake_minimum_required(VERSION 3.14)
project(ecnavdA-yoBemaG)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(modules/fmt)

add_library(ecnavda-yobemag STATIC
	include/arm7tdmidisasm.hpp
	include/gba.hpp
	include/arm7tdmi.hpp
	include/cpu.hpp
	include/hlebios.hpp
	include/apu.hpp
	include/dma.hpp
	include/ppu.hpp
	include/timer.hpp

	src/arm7tdmidisasm.cpp
	src/gba.cpp
	src/arm7tdmi.cpp
	src/cpu.cpp
	src/hlebios.cpp
	src/apu.cpp
	src/dma.cpp
	src/ppu.cpp
	src/timer.cpp
)

target_include_directories(ecnavda-yobemag PUBLIC include)

target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)

target_link_libraries(ecnavda-yobemag PUBLIC fmt)
target_include_directories(ecnavda-yobemag PRIVATE modules/fmt/include)
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(ecnavda-yobemag PRIVATE
		/clang:-ftemplate-depth=5000
		/clang:-fconstexpr-depth=5000
		/clang:-MMD
		/clang:-MP
	)
else ()
	target_compile_options(ecnavda-yobemag PRIVATE
		-ftemplate-depth=5000
		-fconstexpr-depth=5000
		-MMD
		-MP
	)
	target_link_libraries(ecnavda-yobemag PRIVATE
		-lstdc++
		-ldl
		-lm
		-lpthread
		)
endif ()

#if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
#	target_compile_options(ecnavda-yobemag PRIVATE
#		/clang:-ftemplate-depth=5000
#		/clang:-fconstexpr-depth=5000
#		/clang:-MMD
#		/clang:-MP
#		#-Wall
#		#-fsanitize=address
#		#-fsanitize=leak
#		#-fsanitize=pointer-compare
#		#-fsanitize=pointer-subtract
#		#-fstack-protector-all
#		#-fsanitize=undefined
#		#-fsanitize=thread
#	)
#
#	target_link_libraries(ecnavda-yobemag PRIVATE
#		SDL2::SDL2
#		SDL2::SDL2main
#		nfd
#		fmt
#		opengl32
#		#-fsanitize=address
#		#-fsanitize=leak
#		#-fsanitize=pointer-compare
#		#-fsanitize=pointer-subtract
#		#-fstack-protector-all
#		#-fsanitize=undefined
#		#-fsanitize=thread
#		)
#else()
#	target_compile_options(ecnavda-yobemag PRIVATE
#		-ftemplate-depth=5000
#		-fconstexpr-depth=5000
#		-MMD
#		-MP
#		#-Wall
#		#-fsanitize=address
#		#-fsanitize=leak
#		#-fsanitize=pointer-compare
#		#-fsanitize=pointer-subtract
#		#-fstack-protector-all
#		#-fsanitize=undefined
#		#-fsanitize=thread
#		)
#
#	target_link_libraries(ecnavda-yobemag PRIVATE
#		SDL2::SDL2
#		SDL2::SDL2main
#		nfd
#		fmt
#		-lstdc++
#		-ldl
#		-lGL
#		-lm
#		-lpthread
#		#-fsanitize=address
#		#-fsanitize=leak
#		#-fsanitize=pointer-compare
#		#-fsanitize=pointer-subtract
#		#-fstack-protector-all
#		#-fsanitize=undefined
#		#-fsanitize=thread
#		)
#endif()
