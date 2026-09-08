set(E2SIM_LIBRARIES "/usr/local/lib/libe2sim.a")

set(base_examples
    nori-sample
    nori-rc-slicing-demo
    nori-benchmark-scenario
    #oran-interface-example
    #nori-mimo-demo
    #nori-simple-rl-sched
    #nori-embb-urllc-scenario
)

foreach(
  example
  ${base_examples}
)

build_lib_example(
    NAME ${example}
    SOURCE_FILES ${example}.cc
    LIBRARIES_TO_LINK ${libnori}
                      ${libnr}
                      ${liblte}
                      ${E2SIM_LIBRARIES}
  )
endforeach()
