set(source_files
    model/E2-report.cc
    model/E2-interface.cc
    helper/E2-term-helper.cc
    )

set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(header_files
    model/E2-report.h
    model/E2-interface.h
    helper/E2-term-helper.h
    )
    build_lib(
        LIBNAME E2
        SOURCE_FILES ${source_files}
        HEADER_FILES ${header_files}
        LIBRARIES_TO_LINK
          ${libnr}
          ${liboran-interface}
          ${E2SIM_LIBRARIES}
      )
