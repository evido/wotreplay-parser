cmake_minimum_required(VERSION 2.8.11)
project (wotreplay-parser)

set(Boost_USE_MULTITHREADED     ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
set(Boost_USE_STATIC_LIBS       ON)
set(CMAKE_MODULE_PATH           ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/ext/cmake_modules")
set(CMAKE_FIND_FRAMEWORK        LAST)
set(OPENSSL_USE_STATIC          1)
set(CMAKE_AUTOMOC               ON)
set(CMAKE_AUTOUIC               ON)
set(CMAKE_INCLUDE_CURRENT_DIR   ON)

if(USE_BOOST_REGEX STREQUAL 1)
  add_definitions(-DUSE_BOOST_REGEX)
  find_package(Boost 1.50.0 COMPONENTS filesystem program_options system regex REQUIRED)
else()
  find_package(Boost 1.50.0 COMPONENTS filesystem program_options system REQUIRED)
endif()

find_package(PNG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(TBB)
find_package(Qt5Widgets)
find_package(GD)
find_package(Git)


include_directories(AFTER src ext/jsoncpp/include ${Boost_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${INCLUDE_DIRECTORIES})

if(GD_FOUND)
    include_directories(AFTER ${GD_INCLUDE_DIR})
endif()

if(TBB_FOUND)
    include_directories(AFTER ${TBB_INCLUDE_DIR})
endif()

link_directories(${LINK_DIRECTORIES})

if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	set (CMAKE_CXX_FLAGS "/EHsc")
	add_definitions(-DZLIB_WINAPI)
	add_definitions(-D__func__=__FUNCTION__)
	add_definitions(-D__TBB_NO_IMPLICIT_LINKAGE)
	add_definitions(-Dchdir=_chdir)
	ADD_DEFINITIONS(-DBOOST_ALL_NO_LIB)
	# set(CMAKE_GENERATOR_TOOLSET "v140" CACHE STRING "Platform Toolset" FORCE) 
else()
	set (CMAKE_CXX_FLAGS "-Os -std=c++11")
endif()

set(BUILD_SHARED_LIBS				TRUE)
set(CMAKE_CXX_VISIBILITY_PRESET     hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if(TBB_FOUND)
    add_definitions(-DENABLE_TBB)
endif()

set(WOTREPLAY_SOURCES
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
			src/rule.h
			src/class_heatmap_writer.h
			src/tank.h
			src/wotreplay_c.h
			src/packet.cpp 
			src/packet_reader_80.cpp 
			src/parser.cpp 
			src/image_writer.cpp 
			src/game.cpp 
			src/image_util.cpp 
			src/logger.cpp 
			src/json_writer.cpp
			src/arena.cpp
			src/heatmap_writer.cpp
			src/rule.cpp
			src/class_heatmap_writer.cpp
			src/tank.cpp
			src/tinyxml2.cpp
			src/tinyxml2.h
			src/wotreplay_c.cpp)

if (GD_FOUND)
    set(WOTREPLAY_SOURCES ${WOTREPLAY_SOURCES} 
			src/animation_writer.h
			src/animation_writer.cpp)
endif()

add_library(wotreplay SHARED
			${WOTREPLAY_SOURCES})



add_library(jsoncpp STATIC
			ext/jsoncpp/src/jsoncpp.cpp
			ext/jsoncpp/include/json/json.h
			ext/jsoncpp/include/json/json-forwards.h)

include(GenerateExportHeader)
generate_export_header(wotreplay						
						BASE_NAME wotreplay
						EXPORT_MACRO_NAME WOTREPLAY_EXPORT
						EXPORT_FILE_NAME wotreplay_export.h
						STATIC_DEFINE SHARED_EXPORTS_BUILT_AS_STATIC)

set(ENV{TZ} utc)

# the commit's SHA1, and whether the building workspace was dirty or not
execute_process(COMMAND
  "${GIT_EXECUTABLE}" describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SHA1
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the date of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=local
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_DATE
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

# the subject of the commit
execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%s
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_COMMIT_SUBJECT
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND
  "${GIT_EXECUTABLE}" log -1 --format=%ad --date=format-local:%Y%m%d.%H%M%S 
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE BUILD_VERSION
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

string(TIMESTAMP BUILD_DATE "%a %b %d %H:%M:%S %Y" UTC)

unset(ENV{TZ})

# generate version.cpp
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp" @ONLY)

add_executable(wotreplay-parser src/main.cpp "${CMAKE_CURRENT_SOURCE_DIR}/src/version.cpp" src/version.h)

set(LINK_LIBRARIES ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})

if (GD_FOUND)
	add_definitions(-DENABLE_GD)
	set(LINK_LIBRARIES ${LINK_LIBRARIES} ${GD_LIBRARY})
endif()

if (TBB_FOUND)
	set(LINK_LIBRARIES ${LINK_LIBRARIES} ${TBB_LIBRARIES})
endif()

target_link_libraries(wotreplay ${LINK_LIBRARIES} jsoncpp)
target_link_libraries(wotreplay-parser ${LINK_LIBRARIES} wotreplay)


if(Qt5Widgets_FOUND)
	add_library(wotreplay-gui-widgets STATIC
									  src/mainwindow.h
									  src/mainwindow.cpp
									  src/imageviewer.h
									  src/imageviewer.cpp
									  src/mainwindow.ui
									  src/wotreplay-gui.pro)
	qt5_use_modules(wotreplay-gui-widgets Widgets)

	if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	    add_executable(wotreplay-gui WIN32 src/gui_main.cpp)
    else()
        add_executable(wotreplay-gui src/gui_main.cpp)
    endif()

	qt5_use_modules(wotreplay-gui Core Gui Widgets)

	target_link_libraries( wotreplay-gui ${LINK_LIBRARIES} wotreplay-gui-widgets wotreplay Qt5::Widgets)
endif()
