set(source_files
    model/E2-report.cc
    model/E2-interface.cc
    helper/E2-term-helper.cc
    model/asn1c-types.cc
    model/kpm-function-description.cc
    model/kpm-indication.cc
    model/oran-interface.cc
    helper/indication-message-helper.cc
    helper/mmwave-indication-message-helper.cc
    )

include_directories(/usr/local/include/e2sim)

# Tentativa de encontrar a biblioteca em locais comuns, incluindo caminhos multiarch
find_library(E2SIM_LIB 
             NAMES e2sim_shared e2sim
             PATHS /usr/local/lib /usr/local/lib/x86_64-linux-gnu /usr/lib /usr/lib/x86_64-linux-gnu
             NO_DEFAULT_PATH)

if(NOT E2SIM_LIB)
    message(FATAL_ERROR "E2SIM_LIB NOT FOUND! Checked /usr/local/lib, /usr/lib and multiarch paths.")
endif()

message(STATUS "Found E2SIM_LIB: ${E2SIM_LIB}")

set(header_files
    model/E2-report.h
    model/E2-interface.h
    helper/E2-term-helper.h
    model/asn1c-types.h
    model/kpm-function-description.h
    model/kpm-indication.h
    model/oran-interface.h
    helper/indication-message-helper.h
    helper/mmwave-indication-message-helper.h
    model/kpm-metrics-defs.h
    )

    build_lib(
        LIBNAME nori
        SOURCE_FILES ${source_files}
        HEADER_FILES ${header_files}
        LIBRARIES_TO_LINK
          ${libnr}
          ${libcore}
          ${liblte}
          ${E2SIM_LIB}
          sctp
      )