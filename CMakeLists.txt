cmake_minimum_required (VERSION 2.6)
project (WebServer-cpp)

add_compile_options(-std=c++14)

if(WIN32)
link_directories(C:\\OpenSSL-Win64\\lib\\VC)
endif()

include_directories ("${PROJECT_SOURCE_DIR}/HttpServerLibrary" C:\\OpenSSL-Win64\\include)
add_subdirectory (HttpServerLibrary)

add_executable(WebServer WebServer.cpp)
if(WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
target_link_libraries (WebServer HttpServerLibrary libssl64MD libcrypto64MD)
elseif(UNIX)
target_link_libraries (WebServer HttpServerLibrary stdc++ stdc++fs pthread ssl crypto)
endif()

install (TARGETS WebServer DESTINATION bin)