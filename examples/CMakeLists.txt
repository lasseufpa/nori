set(base_examples
    nori-sample
    oran-interface-example
    nori-mimo-demo
    nori-simple-rl-sched
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
