add_executable(game
    src/main.cpp
)

target_link_libraries(game PRIVATE ELE)
add_dependencies(game Shaders)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Engine/assets
            $<TARGET_FILE_DIR:game>/assets
)