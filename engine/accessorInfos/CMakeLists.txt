
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
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/at_oebb.xml ${ACCESSOR_DIR}/at_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/be_brail.xml ${ACCESSOR_DIR}/be_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/ch_sbb.xml ${ACCESSOR_DIR}/ch_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/cz_idnes.xml ${ACCESSOR_DIR}/cz_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/de_db.xml ${ACCESSOR_DIR}/de_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/dk_rejseplanen.xml ${ACCESSOR_DIR}/dk_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/fr_gares.xml ${ACCESSOR_DIR}/fr_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/it_orario.xml ${ACCESSOR_DIR}/it_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/pl_pkp.xml ${ACCESSOR_DIR}/pl_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/sk_imhd.xml ${ACCESSOR_DIR}/sk_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/uk_dundee.xml ${ACCESSOR_DIR}/uk_default.xml)" )
    install( CODE "${SYMLINK} ${ACCESSOR_DIR}/us_septa.xml ${ACCESSOR_DIR}/us_default.xml)" )
endif( UNIX )
