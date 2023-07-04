# ===------------------------------------------------------------------------===#
#
# The KLEE Symbolic Virtual Machine
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
# ===------------------------------------------------------------------------===#
file(GLOB_RECURSE load-call-paths-sources
  "*.cpp"
)

file(GLOB_RECURSE klee-util-sources
  "../klee-util/*.cpp"
)

list(FILTER klee-util-sources EXCLUDE REGEX ".*main\\.cpp$")

add_executable(load-call-paths
  ${load-call-paths-sources}
  ${klee-util-sources}
)

set(KLEE_LIBS
  kleaverExpr
  kleeCore
)

target_include_directories(load-call-paths PRIVATE ../klee-util)
target_link_libraries(load-call-paths ${KLEE_LIBS})

install(TARGETS load-call-paths RUNTIME DESTINATION bin)
