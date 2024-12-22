add_executable(app
    main.cpp
)
target_sources(app PUBLIC FILE_SET CXX_MODULES FILES
    commands/pull.cpp
    consts.cpp
    log.cpp
)