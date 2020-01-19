# file(GLOB NET_SRC
#     "*.cpp"
# )
# file(GLOB NET_HEADER
#     "*.h"
# )
add_library(Net SHARED Socket.cpp SocketListener.cpp ../utility.cpp TLSSocket.cpp TLSSocketListener.cpp Http/Header.cpp Http/Request.cpp Http/Response.cpp Http/HeaderImpl.cpp Http/V1/Connection.cpp Http/V1/HeaderImpl.cpp Http/V1/RequestImpl.cpp Http/V1/ResponseImpl.cpp Http/V1/Http.cpp Http/V2/HeaderImpl.cpp Http/V2/RequestImpl.cpp Http/V2/ResponseImpl.cpp Http/V2/Connection.cpp Http/V2/Frame.cpp Http/V2/Session.cpp Http/V2/Stream.cpp Http/V2/HPack/HPack.cpp)
if(WIN32)
target_link_libraries (Net OpenSSL::SSL OpenSSL::Crypto ws2_32.lib)
elseif(UNIX)
target_link_libraries (Net stdc++ pthread dl OpenSSL::SSL OpenSSL::Crypto)
endif()
install (TARGETS Net DESTINATION bin)
#install (FILES HttpServer.h HttpRequestBuffer.h Http.h Http2.h hpack.h Array.h utility.h DESTINATION include)
