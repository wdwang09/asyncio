add_executable(demo demo.cpp)
target_link_libraries(demo PUBLIC asyncio)

if (WITH_IO)
    add_executable(echo_client echo_client.cpp)
    target_link_libraries(echo_client PUBLIC asyncio)
    add_executable(echo_server echo_server.cpp)
    target_link_libraries(echo_server PUBLIC asyncio)
endif ()
