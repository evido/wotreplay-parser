cmake_minimum_required (VERSION 2.6)
project (wotreplay-parser)
include_directories(AFTER ext/jsoncpp/include ${INCLUDE_DIRECTORIES})
link_directories(${LINK_DIRECTORIES})
set (CMAKE_CXX_FLAGS "-Os -std=c++11 -stdlib=libc++")
SET(EXTRA_LIBS png crypto z boost_filesystem boost_program_options boost_system boost_regex)

add_executable(wotreplay-parser src/main.cpp src/packet.cpp src/packet_reader_80.cpp src/parser.cpp src/image_writer.cpp src/game.cpp src/image_util.cpp src/logger.cpp src/json_writer.cpp ext/jsoncpp/src/jsoncpp.cpp)
TARGET_LINK_LIBRARIES( wotreplay-parser png crypto z boost_filesystem boost_program_options boost_system boost_regex )