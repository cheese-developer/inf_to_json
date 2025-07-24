cmake_minimum_required(VERSION 3.28)
project(inf_to_json LANGUAGES CXX)

# Enable Hot Reload for MSVC if supported
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

find_package(nlohmann_json CONFIG REQUIRED)

# Add source files
add_executable(inf_to_json
    main.cpp
    json.h
    reader.h
    report.h
)

target_sources(inf_to_json
    PRIVATE
        FILE_SET CXX_MODULES FILES
            setup_api.cppm
)

# Require C++23
set_property(TARGET inf_to_json PROPERTY CXX_STANDARD 23)
set_property(TARGET inf_to_json PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET inf_to_json PROPERTY CXX_EXTENSIONS OFF)

# link libraries
target_link_libraries(inf_to_json PRIVATE nlohmann_json::nlohmann_json)
