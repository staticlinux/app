add_executable(app
    main.cpp
)
target_sources(app PUBLIC FILE_SET CXX_MODULES FILES
    commands/pull.cpp
    consts.cpp
    http_client.cpp
    log.cpp
    message_queue.cpp
    read_stream.cpp
    string_utils.cpp
    task_s.cpp
)