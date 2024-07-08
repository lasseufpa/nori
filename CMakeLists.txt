set(source_files
    model/nori-gnb-net-device.cc
    model/nori-E2-report.cc
    model/nori-lte-rlc.cc
    model/nori-lte-rlc-um.cc
    model/nori-lte-rlc-am.cc
    model/nori-rlc-bearer-info.cc
    model/nori-lte-enb-rrc.cc
    helper/nori-helper.cc
    helper/nori-bearer-stats-calculator.cc
    helper/nori-bearer-stats-connector.cc
    #examples/nori-example.cc
    examples/senario-zero.cc
    )

set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(header_files
    model/nori-gnb-net-device.h
    model/nori-E2-report.h
    model/nori-lte-rlc.h
    model/nori-lte-rlc-um.h
    model/nori-lte-rlc-am.h
    model/nori-rlc-bearer-info.h
    model/nori-lte-enb-rrc.h
    helper/nori-helper.h
    helper/nori-bearer-stats-calculator.h
    helper/nori-bearer-stats-connector.h
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
