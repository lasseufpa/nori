set(base_examples
    nori-sample
    E2-example
    e2sim-integration-example
    l3-rrc-example
    oran-interface-example
    ric-control-function-desc
    ric-indication-messages
    test-wrappers
    nori-mimo-demo
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
  )
endforeach()
