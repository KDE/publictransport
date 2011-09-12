
find_package( KDE4 REQUIRED )
add_definitions( ${KDE4_DEFINITIONS} )

set( ACCESSOR_DIR ${DATA_INSTALL_DIR}/plasma_engine_publictransport/accessorInfos )

file( GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.xml"
		 "${CMAKE_CURRENT_SOURCE_DIR}/*.js" )
install( FILES ${files}
    DESTINATION ${ACCESSOR_DIR} )

# Create symlinks to default service providers (one per country)
if( UNIX )
    set( SYMLINK "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink" )
    install( CODE "${SYMLINK} at_oebb.xml ${ACCESSOR_DIR}/at_default.xml)" )
    install( CODE "${SYMLINK} be_brail.xml ${ACCESSOR_DIR}/be_default.xml)" )
    install( CODE "${SYMLINK} ch_sbb.xml ${ACCESSOR_DIR}/ch_default.xml)" )
    install( CODE "${SYMLINK} cz_idnes.xml ${ACCESSOR_DIR}/cz_default.xml)" )
    install( CODE "${SYMLINK} de_db.xml ${ACCESSOR_DIR}/de_default.xml)" )
    install( CODE "${SYMLINK} dk_rejseplanen.xml ${ACCESSOR_DIR}/dk_default.xml)" )
    install( CODE "${SYMLINK} fr_gares.xml ${ACCESSOR_DIR}/fr_default.xml)" )
    install( CODE "${SYMLINK} it_orario.xml ${ACCESSOR_DIR}/it_default.xml)" )
    install( CODE "${SYMLINK} pl_pkp.xml ${ACCESSOR_DIR}/pl_default.xml)" )
    install( CODE "${SYMLINK} se_resrobot.xml ${ACCESSOR_DIR}/se_default.xml)" )
    install( CODE "${SYMLINK} sk_imhd.xml ${ACCESSOR_DIR}/sk_default.xml)" )
    install( CODE "${SYMLINK} uk_dundee.xml ${ACCESSOR_DIR}/uk_default.xml)" )
    install( CODE "${SYMLINK} us_septa.xml ${ACCESSOR_DIR}/us_default.xml)" )
    install( CODE "${SYMLINK} international_flightstats.xml ${ACCESSOR_DIR}/international_default.xml)" )
    install( CODE "${SYMLINK} us_trimet.xml ${ACCESSOR_DIR}/us_trimet.xml)" )
endif( UNIX )
