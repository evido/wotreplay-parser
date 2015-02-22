cmake_minimum_required (VERSION 2.6)
project (wotreplay-parser)

set(Boost_USE_MULTITHREADED		ON)
set(Boost_USE_STATIC_RUNTIME   OFF)

find_package(Boost 1.50.0 COMPONENTS filesystem program_options system regex REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(AFTER ext/jsoncpp/include ${Boost_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} ${PNG_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${INCLUDE_DIRECTORIES})

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
			src/heatmap_writer.h
			src/packet.cpp 
			src/packet_reader_80.cpp 
			src/parser.cpp 
			src/image_writer.cpp 
			src/game.cpp src/image_util.cpp 
			src/logger.cpp 
			src/json_writer.cpp
			src/arena.cpp
			src/heatmap_writer.cpp)

add_library(jsoncpp STATIC 
			ext/jsoncpp/src/jsoncpp.cpp)

add_executable(wotreplay-parser src/main.cpp)

target_link_libraries( wotreplay-parser ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${LIBXML2_LIBRARIES} ${PNG_LIBRARIES} ${ZLIB_LIBRARIES} wotreplay jsoncpp)
