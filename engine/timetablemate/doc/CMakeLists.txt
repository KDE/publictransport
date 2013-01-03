
file( GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.html"
                 "${CMAKE_CURRENT_SOURCE_DIR}/*.png"
                 "${CMAKE_CURRENT_SOURCE_DIR}/*.css" )
install( FILES ${files} DESTINATION ${DOCUMENTATION_DIR} )
