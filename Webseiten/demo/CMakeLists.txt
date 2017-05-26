add_library(ordner SHARED ordner.cpp)
target_link_libraries (ordner HttpServerLibrary)
install (TARGETS ordner DESTINATION web)
