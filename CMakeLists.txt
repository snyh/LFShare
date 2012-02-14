cmake_minimum_required(VERSION 2.8)
find_package(Boost 1.41.0  COMPONENTS serialization iostreams system filesystem)
find_package(Threads)
find_package(OpenSSL)

set(CMAKE_CXX_FLAGS "-std=c++0x")

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
add_executable(tmp 
	src/MFileManager.cpp 
	src/MFile.cpp
	src/main.cpp
	src/NetDriver.cpp
	src/Connector.cpp

	src/rpc_filemanager.cpp
	src/rpc_systeminfo.cpp

	src/AppWebServer/content.cpp
	src/AppWebServer/jrpc.cpp
	src/AppWebServer/request_response.cpp
	src/AppWebServer/server.cpp
	src/AppWebServer/session.cpp
	)
target_link_libraries(tmp ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} pthread crypto jsoncpp)

add_subdirectory(test)