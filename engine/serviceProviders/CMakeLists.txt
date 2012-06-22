
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
    install( CODE "${SYMLINK} at_oebb.pts ${SERVICE_PROVIDER_DIR}/at_default.pts)" )
    install( CODE "${SYMLINK} au_winnipeg.pts ${ACCESSOR_DIR}/au_default.pts)" )
    install( CODE "${SYMLINK} be_brail.pts ${SERVICE_PROVIDER_DIR}/be_default.pts)" )
    install( CODE "${SYMLINK} ch_sbb.pts ${SERVICE_PROVIDER_DIR}/ch_default.pts)" )
    install( CODE "${SYMLINK} cz_idnes.pts ${SERVICE_PROVIDER_DIR}/cz_default.pts)" )
    install( CODE "${SYMLINK} de_db.pts ${SERVICE_PROVIDER_DIR}/de_default.pts)" )
    install( CODE "${SYMLINK} dk_rejseplanen.pts ${SERVICE_PROVIDER_DIR}/dk_default.pts)" )
    install( CODE "${SYMLINK} es_vitoria.pts ${ACCESSOR_DIR}/es_default.pts)" )
    install( CODE "${SYMLINK} fr_gares.pts ${SERVICE_PROVIDER_DIR}/fr_default.pts)" )
    install( CODE "${SYMLINK} hu_szeget.pts ${ACCESSOR_DIR}/hu_default.pts)" )
    install( CODE "${SYMLINK} jp_kyoto.pts ${ACCESSOR_DIR}/jp_default.pts)" )
    install( CODE "${SYMLINK} nz_metlink.pts ${ACCESSOR_DIR}/nz_default.pts)" )
    install( CODE "${SYMLINK} in_bmtc.pts ${ACCESSOR_DIR}/in_default.pts)" )
    install( CODE "${SYMLINK} it_orario.pts ${SERVICE_PROVIDER_DIR}/it_default.pts)" )
    install( CODE "${SYMLINK} pl_pkp.pts ${SERVICE_PROVIDER_DIR}/pl_default.pts)" )
    install( CODE "${SYMLINK} se_resrobot.pts ${SERVICE_PROVIDER_DIR}/se_default.pts)" )
    install( CODE "${SYMLINK} sk_imhd.pts ${SERVICE_PROVIDER_DIR}/sk_default.pts)" )
    install( CODE "${SYMLINK} gb_datagm.pts ${ACCESSOR_DIR}/gb_default.pts)" )
    install( CODE "${SYMLINK} ua_lviv.pts ${ACCESSOR_DIR}/ua_default.pts)" )
    install( CODE "${SYMLINK} us_amtrak.pts ${ACCESSOR_DIR}/us_default.pts)" )
    install( CODE "${SYMLINK} international_flightstats.pts ${SERVICE_PROVIDER_DIR}/international_default.pts)" )
endif( UNIX )
