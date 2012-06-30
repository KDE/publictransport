
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
    install( CODE "${SYMLINK} au_winnipeg.pts ${ACCESSOR_DIR}/au_default.pts)" )
    install( CODE "${SYMLINK} hu_szeget.pts ${ACCESSOR_DIR}/hu_default.pts)" )
    install( CODE "${SYMLINK} jp_kyoto.pts ${ACCESSOR_DIR}/jp_default.pts)" )
    install( CODE "${SYMLINK} nz_metlink.pts ${ACCESSOR_DIR}/nz_default.pts)" )
    install( CODE "${SYMLINK} in_bmtc.pts ${ACCESSOR_DIR}/in_default.pts)" )
    install( CODE "${SYMLINK} it_gtt.pts ${SERVICE_PROVIDER_DIR}/it_default.pts)" )
#     TODO do not override in script/serviceProviders?
    install( CODE "${SYMLINK} es_vitoria.pts ${ACCESSOR_DIR}/es_default.pts)" )
    install( CODE "${SYMLINK} pl_bkm.pts ${SERVICE_PROVIDER_DIR}/pl_default.pts)" )
    install( CODE "${SYMLINK} gb_datagm.pts ${ACCESSOR_DIR}/gb_default.pts)" )
    install( CODE "${SYMLINK} ua_lviv.pts ${ACCESSOR_DIR}/ua_default.pts)" )
    install( CODE "${SYMLINK} us_amtrak.pts ${ACCESSOR_DIR}/us_default.pts)" )
endif( UNIX )
