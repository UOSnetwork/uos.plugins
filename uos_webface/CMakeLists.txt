file(GLOB HEADERS "include/eosio/webface/*.hpp")
add_library( webface
             webface.cpp
             ${HEADERS} )

target_link_libraries( webface appbase fc  )
target_include_directories( webface PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" )
