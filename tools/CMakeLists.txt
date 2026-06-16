# file_to_c - binary to C array converter for resource embedding
add_executable(file_to_c file_to_c.cpp)
set_target_properties(file_to_c PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
