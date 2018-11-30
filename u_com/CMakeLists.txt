file(GLOB HEADERS "include/eosio/u_com/*.hpp")
add_library( u_com
        u_com.cpp
        SimplePocoHandler.cpp
        merkle_tree.cpp
        ${HEADERS} include/eosio/u_com/cvs.h SimplePocoHandler.h)

target_link_libraries( u_com appbase fc http_plugin chain_api_plugin producer_plugin history_api_plugin chain_plugin amqpcpp PocoNet PocoFoundation)
target_include_directories( u_com PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
