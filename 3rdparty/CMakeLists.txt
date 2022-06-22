
# Rapidjson is an header only library
add_library(rapidjson INTERFACE)
target_include_directories(rapidjson INTERFACE rapidjson/include)

# JSON tools library is an interface wrapper for the rapidjson
# library which disable the warnings coming from the rapidjson's headers
# and provides some helper classes
add_library(json INTERFACE)
target_include_directories(json INTERFACE .)
target_link_libraries(json INTERFACE rapidjson)
