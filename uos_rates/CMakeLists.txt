file(GLOB HEADERS "include/eosio/uos_rates/*.hpp")
add_library( uos_rates
        uos_rates.cpp
        ${HEADERS} )

target_link_libraries( uos_rates appbase fc http_plugin chain_api_plugin producer_plugin history_api_plugin chain_plugin singularity)
target_include_directories( uos_rates PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )