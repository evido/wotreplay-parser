cmake_minimum_required (VERSION 2.6)
project (wotreplay-parser)

include_directories(AFTER ext/jsoncpp/include ${INCLUDE_DIRECTORIES})
link_directories(${LINK_DIRECTORIES})

set (CMAKE_CXX_FLAGS "-Os -std=c++11")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()


add_library(wotreplay STATIC 
			src/packet.h
			src/arena.h
			src/game.h
			src/image_util.h
			src/image_writer.h
			src/logger.h
			src/packet.h
			src/parser.h
			src/regex.h
			src/types.h
			src/writer.h
			src/packet_reader.h
			src/packet_reader_80.h
			src/json_writer.h
			src/packet.cpp 
			src/packet_reader_80.cpp 
			src/parser.cpp 
			src/image_writer.cpp 
			src/game.cpp src/image_util.cpp 
			src/logger.cpp 
			src/json_writer.cpp
			src/arena.cpp)

add_library(jsoncpp STATIC 
			ext/jsoncpp/src/jsoncpp.cpp)

add_executable(wotreplay-parser src/main.cpp)
target_link_libraries( wotreplay-parser xml2 png crypto z boost_filesystem boost_program_options boost_system boost_regex wotreplay jsoncpp)