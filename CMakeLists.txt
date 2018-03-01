############################################
cmake_minimum_required(VERSION 2.8.9)
cmake_policy(SET CMP0054 NEW)
project(repromysql)
include(CTest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(INSTALL_DEBUG "On")
set(INSTALL_RELEASE "On")

############################################
# macros
############################################

macro(AddCompilerFlags target flags)
    get_target_property(CF ${target} COMPILE_FLAGS)
    if(CF STREQUAL "CF-NOTFOUND")
        SET(CF "") # set to empty string
    else()
        SET(CF "${CF} ") # a space to cleanly separate from existing content
    endif()

    SET(CF "${CF} ${flags}" )
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS ${CF} )
endmacro()


############################################
# clang support
############################################

set(LINKERFLAGS "")
set(STDLIB "")
set(FLAGS "")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(STDLIB "c++abi")
    set(LINKERFLAGS "-stdlib=libc++ -fcoroutines-ts  -std=c++14")
    set(FLAGS "-stdlib=libc++ -fcoroutines-ts -D_RESUMABLE_FUNCTIONS_SUPPORTED")
endif()

set( CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${LINKERFLAGS}" )


############################################
# OS specific dependencies
############################################

find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)

IF (WIN32)

	set(OSLIBS "Ws2_32" "Rpcrt4")
	find_library(PRIOCPP_LIBRARY NAMES priocpp)
	find_library(PRIOCPPD_LIBRARY NAMES priocppd)
	find_library(REDIS_LIBRARY NAMES hiredis)
	find_library(INTEROP_LIBRARY NAMES Win32_Interop)
	find_library(LIBEVENT_LIBRARY NAMES event)
	find_library(MYSQL_LIBRARY NAMES libmysql)
	find_path(MYSQL_PATH mysql/mysql.h)

	set(BUILDFLAGS "-await ${FLAGS} -I${MYSQL_PATH}/mysql")
	set(DEBUGFLAGS "-DMOL_PROMISE_DEBUG")

	set(LIBS ${PRIOCPP_LIBRARY} ${CRYPTONEAT_LIBRARY} ${GTEST_LIBRARIES} ${OPENSSL_LIBRARIES} ${MYSQL_LIBRARY} ${INTEROP_LIBRARY} ${STDLIB} )

	if( "${CMAKE_BUILD_TYPE}" STREQUAL "Release")
		set(INSTALL_DEBUG "Off")
		set(LIBS ${CMAKE_PROJECT_NAME} ${PRIOCPP_LIBRARY} ${LIBS})
		set(DEBUGFLAGS " ")
	else()
		set(INSTALL_RELEASE "Off")
		set(LIBS ${CMAKE_PROJECT_NAME}d ${PRIOCPPD_LIBRARY} ${LIBS})
		set(DEBUGFLAGS "-DMOL_PROMISE_DEBUG ")
	endif()


 ELSEIF(UNIX)

    set(OSLIBS "pthread")

    find_package(PkgConfig)
    pkg_check_modules(HIREDIS REQUIRED hiredis)
    pkg_check_modules(PRIOCPP REQUIRED priocpp)
    pkg_check_modules(LIBEVENT REQUIRED libevent_pthreads)
    pkg_check_modules(MYSQL REQUIRED mysqlclient)

	set(LIBEVENT_LIBRARY ${LIBEVENT_LIBRARIES})

    set(BUILDFLAGS " ${FLAGS} ${MYSQL_CFLAGS}")
	set(DEBUGFLAGS "-g -DMOL_PROMISE_DEBUG")

	set(LIBS priocppd ${GTEST_LIBRARIES} ${OPENSSL_LIBRARIES} ${MYSQL_LIBRARIES} ${STDLIB} )
ENDIF ()


############################################
# backend: libevent or boost_asio
############################################

set(BACKEND ${LIBEVENT_LIBRARY})

option(WITH_LIBEVENT "use libevent (default:on)" ON)
if(WITH_LIBEVENT)
    add_definitions(-DPROMISE_USE_LIBEVENT)
else()
    add_definitions(-DPROMISE_USE_BOOST_ASIO)
    set(BACKEND "boost_system")
endif()


############################################
# sources
############################################

include_directories(include)
include_directories(${OPENSSL_INCLUDE_DIR})
include_directories(${PRIOCPP_INCLUDE_DIRS})

file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "include/${CMAKE_PROJECT_NAME}/*.h")

############################################
# targets
############################################

add_library(${CMAKE_PROJECT_NAME}  STATIC ${SOURCES})
add_library(${CMAKE_PROJECT_NAME}d STATIC ${SOURCES})

add_executable(Tests t/test.cpp)

AddCompilerFlags(${CMAKE_PROJECT_NAME} ${BUILDFLAGS})

AddCompilerFlags(${CMAKE_PROJECT_NAME}d ${BUILDFLAGS})
AddCompilerFlags(${CMAKE_PROJECT_NAME}d ${DEBUGFLAGS})

AddCompilerFlags(Tests ${BUILDFLAGS})
AddCompilerFlags(Tests ${DEBUGFLAGS})


############################################
# libs
############################################

target_link_libraries(Tests ${CMAKE_PROJECT_NAME}d ${LIBS} ${BACKEND} ${OSLIBS} )


############################################
# tests
############################################

if(WIN32)
	message("skip tests on win32")
else()
	add_test(NAME AllTests COMMAND Tests)
endif()
 
############################################
# install rules
############################################
 
if( "${INSTALL_RELEASE}" STREQUAL "On")
	install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION lib)
endif()

if( "${INSTALL_DEBUG}" STREQUAL "On")
	install(TARGETS ${CMAKE_PROJECT_NAME}d DESTINATION lib)
endif()


install(FILES ${HEADERS} DESTINATION include/${CMAKE_PROJECT_NAME})

install(FILES ${CMAKE_PROJECT_NAME}.pc DESTINATION lib/pkgconfig)
