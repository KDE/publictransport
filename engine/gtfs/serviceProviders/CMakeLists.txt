
find_package( KDE4 REQUIRED )
add_definitions( ${KDE4_DEFINITIONS} )

set( SERVICE_PROVIDER_DIR ${DATA_INSTALL_DIR}/plasma_engine_publictransport/serviceProviders )

file( GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.pts"
                 "${CMAKE_CURRENT_SOURCE_DIR}/*.xml"
                 "${CMAKE_CURRENT_SOURCE_DIR}/*.js" )
install( FILES ${files}
    DESTINATION ${SERVICE_PROVIDER_DIR} )

# Create symlinks to default service providers (one per country)
if( UNIX )
    set( SYMLINK "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink" )
    install( CODE "${SYMLINK} au_winnipeg.pts ${SERVICE_PROVIDER_DIR}/au_default.pts)" )
    install( CODE "${SYMLINK} hu_szeget.pts ${SERVICE_PROVIDER_DIR}/hu_default.pts)" )
    install( CODE "${SYMLINK} jp_kyoto.pts ${SERVICE_PROVIDER_DIR}/jp_default.pts)" )
    install( CODE "${SYMLINK} nz_metlink.pts ${SERVICE_PROVIDER_DIR}/nz_default.pts)" )
    install( CODE "${SYMLINK} in_bmtc.pts ${SERVICE_PROVIDER_DIR}/in_default.pts)" )
    install( CODE "${SYMLINK} it_gtt.pts ${SERVICE_PROVIDER_DIR}/it_default.pts)" )
    install( CODE "${SYMLINK} es_vitoria.pts ${SERVICE_PROVIDER_DIR}/es_default.pts)" )
    install( CODE "${SYMLINK} pl_bkm.pts ${SERVICE_PROVIDER_DIR}/pl_default.pts)" )
    install( CODE "${SYMLINK} gb_datagm.pts ${SERVICE_PROVIDER_DIR}/gb_default.pts)" )
    install( CODE "${SYMLINK} ua_lviv.pts ${SERVICE_PROVIDER_DIR}/ua_default.pts)" )
    install( CODE "${SYMLINK} us_amtrak.pts ${SERVICE_PROVIDER_DIR}/us_default.pts)" )
endif( UNIX )
