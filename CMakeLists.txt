project(ldap)

# search packages used by KDE
find_package (KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(MacroOptionalAddSubdirectory)
include(CheckIncludeFiles)
find_package (KdepimLibs REQUIRED)

find_program(XSLTPROC_EXECUTABLE xsltproc)
macro_log_feature(XSLTPROC_EXECUTABLE "xsltproc" "The command line XSLT processor from libxslt" "http://xmlsoft.org/XSLT/" FALSE "" "Needed for building Akonadi resources. Recommended.")

if (XSLTPROC_EXECUTABLE)
  # generates a D-Bus interface description from a KConfigXT file
  macro( kcfg_generate_dbus_interface _kcfg _name )
    add_custom_command(
      OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_name}.xml
      COMMAND ${XSLTPROC_EXECUTABLE} --stringparam interfaceName ${_name}
              ${KDEPIMLIBS_DATA_DIR}/akonadi-kde/kcfg2dbus.xsl
              ${_kcfg}
              > ${CMAKE_CURRENT_BINARY_DIR}/${_name}.xml
      DEPENDS ${KDEPIMLIBS_DATA_DIR}/akonadi-kde/kcfg2dbus.xsl
              ${_kcfg}
    )
  endmacro( kcfg_generate_dbus_interface )
endif (XSLTPROC_EXECUTABLE)

if(WIN32)
    set(LIB_INSTALL_DIR ${LIB_INSTALL_DIR}
                        RUNTIME DESTINATION ${BIN_INSTALL_DIR}
                        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
                        ARCHIVE DESTINATION ${LIB_INSTALL_DIR} )
endif(WIN32)

set(KDE4_ICON_DIR ${KDE4_INSTALL_DIR}/share/icons)

include_directories(
    ${KDE4_INCLUDES}
    ${KDEPIMLIBS_INCLUDE_DIRS}
)

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}" )


########### next target ###############

set( ldapresource_SRCS retrieveitemsjob.cpp retrieveitemjob.cpp ldapmapper.cpp retrievegroupsjob.cpp retrievegroupmembersjob.cpp
     retrieveupdatesjob.cpp updateitemjob.cpp incrementalupdatejob.cpp incrementalupdatedata.cpp updategroupjob.cpp
     settingswidget.cpp )

kde4_add_ui_files(ldapresource_SRCS settingswidget.ui)

install( FILES ldapresource.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents" )

kde4_add_kcfg_files(ldapresource_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/ldapresource.kcfg org.kde.Akonadi.LDAP.Settings)
qt4_add_dbus_adaptor(ldapresource_SRCS
  ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.LDAP.Settings.xml settings.h Settings
)

kde4_add_executable(akonadi_ldap_resource RUN_UNINSTALLED ldapresource.cpp ${ldapresource_SRCS})

target_link_libraries(akonadi_ldap_resource ${KDE4_AKONADI_LIBS} ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KABC_LIBS} ${KDEPIMLIBS_AKONADI_KABC_LIBS} ${KDEPIMLIBS_KLDAP_LIBS})

install(TARGETS akonadi_ldap_resource ${INSTALL_TARGETS_DEFAULT_ARGS})

kde4_add_executable(ldaptest RUN_UNINSTALLED main.cpp ${ldapresource_SRCS})
target_link_libraries(ldaptest ${KDE4_AKONADI_LIBS} ${QT_QTCORE_LIBRARY} ${QT_QTDBUS_LIBRARY} ${KDE4_KDECORE_LIBS} ${KDE4_KABC_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBS} ${KDEPIMLIBS_KLDAP_LIBS})
