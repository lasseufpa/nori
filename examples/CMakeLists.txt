set(base_examples
    E2-example
)

foreach(
  example
  ${base_examples}
)

build_lib_example(
    NAME ${example}
    SOURCE_FILES ${example}.cc
    LIBRARIES_TO_LINK ${libE2}
                      ${libnr}
                      ${liblte}
                      ${liboran-interface}
  )
endforeach()
