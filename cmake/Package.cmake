set(bundle "${BINARY_DIR}/SimpleMultiAnalyzer_artefacts/${CONFIG}/VST3/simple_multi_analyzer.vst3")
if(NOT EXISTS "${bundle}/Contents/x86_64-win/simple_multi_analyzer.vst3")
    message(FATAL_ERROR "The Windows x64 VST3 bundle is missing: ${bundle}")
endif()
set(stage "${BINARY_DIR}/package/${CONFIG}")
file(MAKE_DIRECTORY "${stage}/binary" "${stage}/source/third_party" "${BINARY_DIR}/dist")
file(COPY "${bundle}" DESTINATION "${stage}/binary")
foreach(name README.md LICENSE THIRD_PARTY.md)
    file(COPY "${SOURCE_DIR}/${name}" DESTINATION "${stage}/binary")
endforeach()
foreach(name CMakeLists.txt .clang-format .gitignore .github Source cmake README.md LICENSE THIRD_PARTY.md)
    file(COPY "${SOURCE_DIR}/${name}" DESTINATION "${stage}/source")
endforeach()
file(COPY "${JUCE_SOURCE_DIR}/" DESTINATION "${stage}/source/third_party/JUCE")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${BINARY_DIR}/dist/simple_multi_analyzer-windows-x64.zip"
        --format=zip simple_multi_analyzer.vst3 README.md LICENSE THIRD_PARTY.md
    WORKING_DIRECTORY "${stage}/binary" COMMAND_ERROR_IS_FATAL ANY)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${BINARY_DIR}/dist/simple_multi_analyzer-source.zip" --format=zip source
    WORKING_DIRECTORY "${stage}" COMMAND_ERROR_IS_FATAL ANY)
