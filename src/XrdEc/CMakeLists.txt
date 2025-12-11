if(NOT BUILD_XRDEC)
  return()
endif()

add_library(XrdEc SHARED
  XrdEcConfig.hh
  XrdEcObjCfg.hh
  XrdEcReader.hh              XrdEcReader.cc
  XrdEcRedundancyProvider.hh  XrdEcRedundancyProvider.cc
  XrdEcStrmWriter.hh          XrdEcStrmWriter.cc
  XrdEcThreadPool.hh
  XrdEcUtilities.hh           XrdEcUtilities.cc
  XrdEcWrtBuff.hh
)

target_link_libraries(XrdEc PRIVATE XrdCl XrdUtils ${ISAL_LIBRARIES})
target_include_directories(XrdEc PRIVATE ${ISAL_INCLUDE_DIRS})

set_target_properties(XrdEc
  PROPERTIES VERSION 1.0.0 SOVERSION 1)

install(
  FILES
    XrdEcConfig.hh
    XrdEcObjCfg.hh
    XrdEcReader.hh
    XrdEcRedundancyProvider.hh
    XrdEcStrmWriter.hh
    XrdEcThreadPool.hh
    XrdEcUtilities.hh
    XrdEcWrtBuff.hh
  DESTINATION
    ${CMAKE_INSTALL_INCLUDEDIR}/xrootd/private/XrdEc
)

install(TARGETS XrdEc LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# ----------------------------------------------------------------------
# Client-side EC plugin (built separately to avoid XrdCl<->XrdEc cycles)
# ----------------------------------------------------------------------
if(BUILD_XRDEC)
  add_library(XrdEcDefault SHARED
    ../XrdCl/XrdClEcHandler.cc
    ../XrdCl/XrdClEcHandler.hh
    XrdEcDefaultPlugin.cc
  )

  target_include_directories(XrdEcDefault PRIVATE ../XrdCl ${ISAL_INCLUDE_DIRS})
  target_link_libraries(XrdEcDefault PRIVATE XrdEc XrdCl XrdUtils ${ISAL_LIBRARIES})
  set_target_properties(XrdEcDefault PROPERTIES VERSION 1.0.0 SOVERSION 1)

  install(TARGETS XrdEcDefault LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
