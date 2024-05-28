set(source_files
    model/nori-gnb-net-device.cc
    model/nori-E2-report.cc
    )

set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(header_files
    model/nori-gnb-net-device.h
    model/nori-E2-report.h
    )
    build_lib(
        LIBNAME nori
        SOURCE_FILES ${source_files}
        HEADER_FILES ${header_files}
        LIBRARIES_TO_LINK
          ${liblte}
          ${libinternet-apps}
          ${libflow-monitor}
          ${libnr}
          ${liboran-interface}
          ${E2SIM_LIBRARIES}
          ${libcore}
      )
