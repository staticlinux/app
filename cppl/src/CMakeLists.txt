add_library(cppl)

target_sources(cppl PUBLIC FILE_SET CXX_MODULES FILES
    core/task.cpp
    core/module.cpp
    module.cpp
)