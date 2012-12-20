
find_package( KDE4 REQUIRED )
add_definitions( ${KDE4_DEFINITIONS} )

# Install all base script files in the current source directory
file( GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.js" )
install( FILES ${files} DESTINATION ${SERVICE_PROVIDER_DIR} )
