cmake_minimum_required(VERSION 3.22)
project(JoeysLabPlugin VERSION 0.1.0)

# Path to your JUCE folder (default: ~/JUCE)
set(JUCE_DIR "$ENV{HOME}/JUCE" CACHE PATH "JUCE folder")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(JoeysLab
    COMPANY_NAME "JoeysLab"
    BUNDLE_ID "com.joeyslab.plugin"
    PLUGIN_MANUFACTURER_CODE Joey
    PLUGIN_CODE Jlab
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "JoeysLab"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    NEEDS_WEB_BROWSER TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "JoeysLab records ideas and vocals for the sampler.")

target_sources(JoeysLab PRIVATE Source/Plugin.cpp)
target_compile_features(JoeysLab PRIVATE cxx_std_17)
target_compile_definitions(JoeysLab PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(JoeysLab
    PRIVATE juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Bundle the JoeysLab web app (index.html + samples) inside each plugin
set(WEB_SRC "${CMAKE_CURRENT_SOURCE_DIR}/..")
foreach(fmt VST3 AU Standalone)
  if(TARGET JoeysLab_${fmt})
    add_custom_command(TARGET JoeysLab_${fmt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:JoeysLab_${fmt}>/Contents/Resources/web/samples"
      COMMAND ${CMAKE_COMMAND} -E copy "${WEB_SRC}/index.html" "${WEB_SRC}/icon.svg" "$<TARGET_BUNDLE_DIR:JoeysLab_${fmt}>/Contents/Resources/web/"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${WEB_SRC}/samples" "$<TARGET_BUNDLE_DIR:JoeysLab_${fmt}>/Contents/Resources/web/samples"
      VERBATIM)
  endif()
endforeach()
