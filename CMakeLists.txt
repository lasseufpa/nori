set(source_files
    model/oran-interface.cc
    # model/e2sim-mod.cc
    # model/nori-e2-agent.cc
    # model/nori-encode-e2ap.cc
    )

include_directories(/usr/local/include/e2sim)

#Desativa a necessidade do oer

add_definitions(-DASN_DISABLE_OER_SUPPORT)

set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(header_files
    model/oran-interface.h
    model/e2sim-mod.h
    # model/nori-e2-agent.h
    # model/nori-encode-e2ap.h
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
