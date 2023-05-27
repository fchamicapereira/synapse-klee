#===------------------------------------------------------------------------===#
#
#                     The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#
file(GLOB_RECURSE synapse-sources
  "*.cpp"
)

file(GLOB_RECURSE load-call-paths-sources
  "../load-call-paths*.cpp"
)

file(GLOB_RECURSE call-paths-to-bdd-sources
  "../call-paths-to-bdd/*.cpp"
)

file(GLOB_RECURSE bdd-reorderer-sources
  "../bdd-reorderer/*.cpp"
)

file(GLOB_RECURSE clone-sources
  "../clone/*.cpp"
)

file(GLOB_RECURSE klee-util-sources
  "../klee-util/*.cpp"
)

list(FILTER synapse-sources EXCLUDE REGEX ".*boilerplate.*$")
list(FILTER load-call-paths-sources EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER call-paths-to-bdd-sources EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER bdd-reorderer-sources EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER clone-sources EXCLUDE REGEX ".*clone\\.cpp$")
list(FILTER klee-util-sources EXCLUDE REGEX ".*main\\.cpp$")

add_executable(synapse
  ${synapse-sources}
  ${load-call-paths-sources}
  ${bdd-reorderer-sources}
  ${call-paths-to-bdd-sources}
  ${clone-sources}
  ${klee-util-sources}
)

set(KLEE_LIBS
  kleaverExpr
  kleeCore
)

target_include_directories(synapse PRIVATE ../load-call-paths ../call-paths-to-bdd ../klee-util ../clone)
target_link_libraries(synapse ${KLEE_LIBS})

install(TARGETS synapse RUNTIME DESTINATION bin)
