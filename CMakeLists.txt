set(source_files
    model/E2-report.cc
    model/E2-interface.cc
    helper/E2-term-helper.cc
    model/asn1c-types.cc
    model/function-description.cc
    model/kpm-function-description.cc
    model/kpm-indication.cc
    model/oran-interface.cc
    model/ric-control-function-description.cc
    model/ric-control-message.cc
    helper/indication-message-helper.cc
    helper/lte-indication-message-helper.cc
    helper/mmwave-indication-message-helper.cc
    helper/oran-interface-helper.cc
    )

include_directories(/usr/local/include/e2sim)
set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(header_files
    model/E2-report.h
    model/E2-interface.h
    helper/E2-term-helper.h
    model/asn1c-types.h
    model/function-description.h
    model/kpm-function-description.h
    model/kpm-indication.h
    model/oran-interface.h
    model/ric-control-function-description.h
    model/ric-control-message.h
    helper/indication-message-helper.h
    helper/lte-indication-message-helper.h
    helper/mmwave-indication-message-helper.h
    helper/oran-interface-helper.h
    )
    set(test_sources
    test/oran-interface-test-suite.cc)

    build_lib(
        LIBNAME nori
        SOURCE_FILES ${source_files}
        HEADER_FILES ${header_files}
        LIBRARIES_TO_LINK
          ${libnr}
          ${E2SIM_LIBRARIES}
          ${libcore}
          ${liblte}
        TEST_SOURCES ${test_sources}
      )
