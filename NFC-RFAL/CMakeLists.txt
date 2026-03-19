FILE(GLOB library_sources src/*.c src/*.cpp)

set(includedirs
  src
  )

set(requires arduino)

idf_component_register(INCLUDE_DIRS ${includedirs} SRCS ${library_sources} REQUIRES ${requires})
