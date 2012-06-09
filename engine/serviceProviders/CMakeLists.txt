
find_package( KDE4 REQUIRED )
add_definitions( ${KDE4_DEFINITIONS} )

set( SERVICE_PROVIDER_DIR ${DATA_INSTALL_DIR}/plasma_engine_publictransport/serviceProviders )
# TODO Rename to "/serviceProviders"

file( GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.pts"
                 "${CMAKE_CURRENT_SOURCE_DIR}/*.xml"
                 "${CMAKE_CURRENT_SOURCE_DIR}/*.js" )
install( FILES ${files}
    DESTINATION ${SERVICE_PROVIDER_DIR} )

# Create symlinks to default service providers (one per country)
if( UNIX )
    set( SYMLINK "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink" )
    install( CODE "${SYMLINK} at_oebb.xml ${SERVICE_PROVIDER_DIR}/at_default.xml)" )
    install( CODE "${SYMLINK} be_brail.xml ${SERVICE_PROVIDER_DIR}/be_default.xml)" )
    install( CODE "${SYMLINK} ch_sbb.xml ${SERVICE_PROVIDER_DIR}/ch_default.xml)" )
    install( CODE "${SYMLINK} cz_idnes.xml ${SERVICE_PROVIDER_DIR}/cz_default.xml)" )
    install( CODE "${SYMLINK} de_db.xml ${SERVICE_PROVIDER_DIR}/de_default.xml)" )
    install( CODE "${SYMLINK} dk_rejseplanen.xml ${SERVICE_PROVIDER_DIR}/dk_default.xml)" )
    install( CODE "${SYMLINK} fr_gares.xml ${SERVICE_PROVIDER_DIR}/fr_default.xml)" )
    install( CODE "${SYMLINK} it_orario.xml ${SERVICE_PROVIDER_DIR}/it_default.xml)" )
    install( CODE "${SYMLINK} pl_pkp.xml ${SERVICE_PROVIDER_DIR}/pl_default.xml)" )
    install( CODE "${SYMLINK} se_resrobot.xml ${SERVICE_PROVIDER_DIR}/se_default.xml)" )
    install( CODE "${SYMLINK} sk_imhd.xml ${SERVICE_PROVIDER_DIR}/sk_default.xml)" )
    install( CODE "${SYMLINK} uk_dundee.xml ${SERVICE_PROVIDER_DIR}/uk_default.xml)" )
    install( CODE "${SYMLINK} us_septa.xml ${SERVICE_PROVIDER_DIR}/us_default.xml)" )
    install( CODE "${SYMLINK} international_flightstats.xml ${SERVICE_PROVIDER_DIR}/international_default.xml)" )
endif( UNIX )
