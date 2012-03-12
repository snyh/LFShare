cmake_minimum_required(VERSION 2.8)

if(WIN32)
	set(CMAKE_CXX_FLAGS "-static -std=gnu++0x -DBOOST_THREAD_USE_LIB")
else(WIN32)
	find_package(Boost COMPONENTS thread serialization iostreams system filesystem)
	find_package(Threads)
	find_package(OpenSSL)
	set(CMAKE_CXX_FLAGS "-static -std=c++0x -DAWS_DEBUG")
endif(WIN32)

include_directories("3dlib")

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
add_executable(server
	src/pre.hpp.gch
	src/FInfoManager.cpp
	src/NetDriver.cpp
	src/Transport.cpp
	src/Dispatcher.cpp
	src/CoreStruct.cpp
	src/NativeFile.cpp
	src/tools.cpp
	src/main.cpp

	src/UI/rpc_filemanager.cpp
	src/UI/rpc_systeminfo.cpp

	../AppWebServer/content.cpp
	../AppWebServer/jrpc.cpp
	../AppWebServer/request_response.cpp
	../AppWebServer/server.cpp
	../AppWebServer/session.cpp
	)

if(WIN32) 
	target_link_libraries( server 
		boost_system-mt
		boost_filesystem-mt
		boost_iostreams-mt
		boost_thread_win32-mt
		crypto 
		jsoncpp
		comdlg32 oleaut32 imm32 winmm winspool kernel32 user32 shell32 uuid ole32 advapi32 ws2_32 mswsock
		) 
else(WIN32)
	target_link_libraries(server ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} pthread crypto jsoncpp) 
endif(WIN32)


#add_subdirectory(test)
