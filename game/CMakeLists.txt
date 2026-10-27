add_executable(game
    src/test.cpp
    src/AI/EnemyBehaviorCommand.cpp
    src/AI/MoveToCommand.cpp
    src/AI/PatrolCommand.cpp
)

target_link_libraries(game PRIVATE ELE)
add_dependencies(game Shaders)
find_program(NIX_EXECUTABLE nix)

find_program(NIX_INSTANTIATE_EXECUTABLE nix-instantiate)

if(NIX_INSTANTIATE_EXECUTABLE)
    execute_process(
        COMMAND ${NIX_INSTANTIATE_EXECUTABLE} --eval -E "(import <nixpkgs> {}).alsa-lib.outPath"
        OUTPUT_VARIABLE ALSA_LIB_PATH_RAW
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NIX_EVAL_RESULT
        ERROR_VARIABLE NIX_EVAL_ERROR
    )
    string(REPLACE "\"" "" ALSA_LIB_PATH "${ALSA_LIB_PATH_RAW}")

    if(NIX_EVAL_RESULT EQUAL 0 AND EXISTS "${ALSA_LIB_PATH}/lib")
        message(STATUS "Found alsa-lib via Nix: ${ALSA_LIB_PATH}/lib")
        set_target_properties(game PROPERTIES
            BUILD_RPATH "${ALSA_LIB_PATH}/lib"
            INSTALL_RPATH "${ALSA_LIB_PATH}/lib"
        )
    else()
        message(STATUS "Nix eval for alsa-lib failed or path not found — skipping RPATH injection")
        message(STATUS "Nix eval result: ${NIX_EVAL_RESULT}")
        message(STATUS "Nix eval error output: ${NIX_EVAL_ERROR}")
    endif()
else()
    message(STATUS "nix-instantiate not found — assuming system libraries are in standard paths")
endif()

target_include_directories(ELE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Engine/assets
            $<TARGET_FILE_DIR:game>/assets
)