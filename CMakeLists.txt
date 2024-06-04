# CMakeList.txt : CMake project for MCPPServerSoftware, include source and define
# project specific logic here.
#
project(MCPPServer)

cmake_minimum_required (VERSION 3.22)

find_package(OpenSSL REQUIRED)

include_directories(MCPPServer)

add_executable(MCPPServer MCPPServerSoftware.cpp MCPPServerSoftware.h io/configs.cpp io/configs.h io/json.cpp io/json.h io/logger.cpp io/logger.h networking/packets/packet.h
networking/packets/packet.cpp server/client.cpp server/client.h server/minecraft_server.cpp server/minecraft_server.h threads/thread_pool.cpp threads/thread_pool.h
util/component/text_components.cpp util/component/text_components.h util/identifiers/indentifier.h util/identifiers/uuid.h minecraft/chat/commands.cpp minecraft/chat/commands.h networking/hash.h networking/hash.cpp)

# TODO: Add tests and install targets if needed.
target_link_libraries(MCPPServer OpenSSL::SSL OpenSSL::Crypto)