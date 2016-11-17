cmake_minimum_required (VERSION 2.6)
project (WebServer-cpp)

add_compile_options(-std=c++14)

if(WIN32)
include_directories(C:\\OpenSSL-Win64\\include)
link_directories(C:\\OpenSSL-Win64\\lib\\VC)
endif()

include_directories("${PROJECT_SOURCE_DIR}/HttpServerLibrary")
add_subdirectory(HttpServerLibrary)
add_subdirectory(DynamischeSeiten)

add_executable(WebServer WebServer.cpp)
if(WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
target_link_libraries (WebServer HttpServerLibrary libssl64MD libcrypto64MD)
elseif(UNIX)
set_property(TARGET WebServer PROPERTY INSTALL_RPATH $ORIGIN)
target_link_libraries (WebServer HttpServerLibrary stdc++ stdc++fs pthread dl ssl crypto)
endif()

install (TARGETS WebServer DESTINATION bin)