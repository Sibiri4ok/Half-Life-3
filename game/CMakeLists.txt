file(GLOB_RECURSE SOURCES_CXX *.cpp)
list(REMOVE_ITEM SOURCES_CXX "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

add_library(game ${SOURCES_CXX})
target_link_libraries(game PUBLIC engine)
target_include_directories(game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(main main.cpp)
target_compile_features(main PRIVATE cxx_std_17)
target_link_libraries(main PRIVATE game)

# Link X11 library for XInitThreads on Linux
if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	target_link_libraries(main PRIVATE ${X11_LIBRARIES})
	target_include_directories(main PRIVATE ${X11_INCLUDE_DIR})
endif()
