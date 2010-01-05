
find_package(KDE4 REQUIRED)
add_definitions (${KDE4_DEFINITIONS})

file(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.xml")
install(FILES ${files}
    DESTINATION ${KDE4_DATA_INSTALL_DIR}/plasma_engine_publictransport/accessorInfos/)