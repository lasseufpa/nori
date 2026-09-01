set(source_files                                                                                                                                                                                                                     
    model/oran-interface.cc                                                                                                                                                                                                          
    model/asn1c-types.cc                                                                                                                                                                                                             
    model/kpm-indication.cc                                                                                                                                                                                                          
    model/kpm-function-description.cc                                                                                                                                                                                                
    model/E2-interface.cc                                                                                                                                                                                                            
    model/E2-report.cc                                                                                                                                                                                                               
    model/function-description.cc                                                                                                                                                                                                    
    model/nr-mac-scheduler-ue-info-rl.cc                                                                                                                                                                                             
    model/nr-rl-mac-scheduler-ofdma.cc
    model/ric-control-function-description.cc
    helper/indication-message-helper.cc
    helper/nori-indication-message-helper.cc
    helper/nori-slicing-helper.cc
    helper/E2-term-helper.cc
    helper/lte-indication-message-helper.cc
    )

include_directories(/usr/local/include/e2sim)

#Desativa a necessidade do oer

add_definitions(-DASN_DISABLE_OER_SUPPORT)

set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(header_files
    model/oran-interface.h
    model/e2sim-mod.h
    model/asn1c-types.h
    model/kpm-indication.h
    model/kpm-function-description.h
    model/ric-control-function-description.h
    model/E2-interface.h
    model/E2-report.h
    model/function-description.h
    model/nr-mac-scheduler-ue-info-rl.h
    model/nr-rl-mac-scheduler-ofdma.h
    helper/indication-message-helper.h
    helper/nori-indication-message-helper.h
    helper/nori-slicing-helper.h
    helper/E2-term-helper.h
    helper/lte-indication-message-helper.h
    )

build_lib(
    LIBNAME nori
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK
      ${libnr}
      ${E2SIM_LIBRARIES}
      ${libcore}
      ${liblte}
  )
