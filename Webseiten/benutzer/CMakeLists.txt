add_library(benutzer SHARED benutzer.cpp)
target_link_libraries (benutzer HttpServerLibrary)
install (TARGETS benutzer DESTINATION web)
