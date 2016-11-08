add_library(HttpServerLibrary STATIC HttpServer.cpp RecvBuffer.cpp HttpHeader.cpp DualByteArray.cpp ByteArray.cpp)
install (TARGETS HttpServerLibrary DESTINATION bin)
install (FILES HttpServer.h RecvBuffer.h HttpHeader.h DualByteArray.h ByteArray.h DESTINATION include)
