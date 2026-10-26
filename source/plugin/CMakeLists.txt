# MAC-4: macOS also builds the Audio Unit (Logic and GarageBand load only AUs)
if(APPLE)
    set(MS2K_PLUGIN_FORMATS VST3 AU)
else()
    set(MS2K_PLUGIN_FORMATS VST3)
endif()
# MS2000R VST3 (phase 1; phase 2 = VST3-2: the vector panel editor, state in the project). Built only with -DMS2K_BUILD_VST3=ON, in its own build folder (build_vst).
juce_add_plugin(MS2000R_VST
    COMPANY_NAME "MS2000 emulation project"
    PRODUCT_NAME "MS2000R"
    PLUGIN_MANUFACTURER_CODE Msem
    PLUGIN_CODE Ms2r
    FORMATS ${MS2K_PLUGIN_FORMATS}
    BUNDLE_ID io.github.ms2000r-emu.plugin
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth)

juce_generate_juce_header(MS2000R_VST)

target_sources(MS2000R_VST PRIVATE PluginProcessor.cpp PluginEditor.cpp)
target_include_directories(MS2000R_VST PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/source)
target_compile_definitions(MS2000R_VST PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(MS2000R_VST PRIVATE ms2000
    juce::juce_audio_utils juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# A console host that loads the built .vst3 like a DAW does and renders a note through processBlock.
juce_add_console_app(MS2000R_HostTest PRODUCT_NAME "MS2000R_HostTest")
juce_generate_juce_header(MS2000R_HostTest)
target_sources(MS2000R_HostTest PRIVATE HostTest.cpp)
target_compile_definitions(MS2000R_HostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(MS2000R_HostTest PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# FLASHDUMP-1 (2026-10-08): MS2000 Flash Tool - installs unknown-technologies' flash-dump update and reads the 1 MB
# flash of a real MS2000 / MS2000R over MIDI (source/tools/flashdump, protocol in ms2k_ipl, tested by ipl_e2e).
juce_add_gui_app(MS2000FlashTool PRODUCT_NAME "MS2000 Flash Tool" COMPANY_NAME "MS2000 emulation project")
juce_generate_juce_header(MS2000FlashTool)
target_sources(MS2000FlashTool PRIVATE ${CMAKE_SOURCE_DIR}/source/tools/flashdump/FlashToolApp.cpp)
target_include_directories(MS2000FlashTool PRIVATE ${CMAKE_SOURCE_DIR}/source)
target_compile_definitions(MS2000FlashTool PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(MS2000FlashTool PRIVATE ms2k_ipl juce::juce_gui_basics juce::juce_audio_devices
    PUBLIC juce::juce_recommended_config_flags)
# ... and the emulated MS2000R on two MIDI ports, to try the tool without a real unit (loopMIDI)
juce_add_console_app(MS2000IplBridge PRODUCT_NAME "MS2000IplBridge")
juce_generate_juce_header(MS2000IplBridge)
target_sources(MS2000IplBridge PRIVATE ${CMAKE_SOURCE_DIR}/source/tools/flashdump/IplBridge.cpp)
target_include_directories(MS2000IplBridge PRIVATE ${CMAKE_SOURCE_DIR}/source)
target_compile_definitions(MS2000IplBridge PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MS2000IplBridge PRIVATE ms2000 juce::juce_audio_devices PUBLIC juce::juce_recommended_config_flags)

# MULTI-1 (2026-10-02): several instances per process. The host loads this small VST3 module; the JUCE plugin above
# travels in the bundle as Contents/Resources/MS2000R_engine.dll (macOS: MS2000R_engine) and every instance after the first runs from its
# own copy of it (see shim/ms2k_vst3_shim.cpp). Result: build_vst/VST3_multi/MS2000R.vst3
if(WIN32)
    set(MS2K_VST3SDK ${JUCE_MODULES_DIR}/juce_audio_processors/format_types/VST3_SDK)   # the VST3 SDK headers JUCE carries
    add_library(MS2000R_Shim SHARED shim/ms2k_vst3_shim.cpp
        ${MS2K_VST3SDK}/pluginterfaces/base/coreiids.cpp ${MS2K_VST3SDK}/pluginterfaces/base/funknown.cpp)
    target_include_directories(MS2000R_Shim PRIVATE ${MS2K_VST3SDK})
    target_link_libraries(MS2000R_Shim PRIVATE ole32)
    set_target_properties(MS2000R_Shim PROPERTIES OUTPUT_NAME MS2000R SUFFIX .vst3)
    add_dependencies(MS2000R_Shim MS2000R_VST_VST3)
    set(MS2K_MULTI ${CMAKE_BINARY_DIR}/VST3_multi/MS2000R.vst3)
    add_custom_command(TARGET MS2000R_Shim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MS2K_MULTI}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MS2K_MULTI}/Contents/x86_64-win ${MS2K_MULTI}/Contents/Resources
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MS2000R_VST_VST3> ${MS2K_MULTI}/Contents/Resources/MS2000R_engine.dll
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MS2000R_Shim> ${MS2K_MULTI}/Contents/x86_64-win/MS2000R.vst3)
elseif(APPLE)
    # MAC-4: the same on macOS - JUCE's bundle with its binary moved to Contents/Resources/MS2000R_engine and the
    # loader module as Contents/MacOS/MS2000R. Ad-hoc signed (the engine first: it is code outside Contents/MacOS).
    set(MS2K_VST3SDK ${JUCE_MODULES_DIR}/juce_audio_processors/format_types/VST3_SDK)
    add_library(MS2000R_Shim MODULE shim/ms2k_vst3_shim.cpp
        ${MS2K_VST3SDK}/pluginterfaces/base/coreiids.cpp ${MS2K_VST3SDK}/pluginterfaces/base/funknown.cpp)
    target_include_directories(MS2000R_Shim PRIVATE ${MS2K_VST3SDK})
    target_compile_features(MS2000R_Shim PRIVATE cxx_std_17)
    set_target_properties(MS2000R_Shim PROPERTIES CXX_VISIBILITY_PRESET hidden PREFIX "" SUFFIX "" OUTPUT_NAME MS2000R_shim)
    target_link_libraries(MS2000R_Shim PRIVATE "-framework CoreFoundation")
    add_dependencies(MS2000R_Shim MS2000R_VST_VST3)
    set(MS2K_MULTI ${CMAKE_BINARY_DIR}/VST3_multi/MS2000R.vst3)
    add_custom_command(TARGET MS2000R_Shim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MS2K_MULTI}
        COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:MS2000R_VST_VST3> ${MS2K_MULTI}
        COMMAND ${CMAKE_COMMAND} -E rename ${MS2K_MULTI}/Contents/MacOS/MS2000R ${MS2K_MULTI}/Contents/Resources/MS2000R_engine
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MS2000R_Shim> ${MS2K_MULTI}/Contents/MacOS/MS2000R
        COMMAND xattr -cr ${MS2K_MULTI}
        COMMAND codesign --force --sign - ${MS2K_MULTI}/Contents/Resources/MS2000R_engine
        COMMAND codesign --force --sign - ${MS2K_MULTI})

    # ... and the Audio Unit the same way (shim/ms2k_au_shim.cpp). Result: build_vst/AU_multi/MS2000R.component
    add_library(MS2000R_AUShim MODULE shim/ms2k_au_shim.cpp)
    target_compile_features(MS2000R_AUShim PRIVATE cxx_std_17)
    set_target_properties(MS2000R_AUShim PROPERTIES CXX_VISIBILITY_PRESET hidden PREFIX "" SUFFIX "" OUTPUT_NAME MS2000R_au_shim)
    target_link_libraries(MS2000R_AUShim PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
    add_dependencies(MS2000R_AUShim MS2000R_VST_AU)
    set(MS2K_AU_MULTI ${CMAKE_BINARY_DIR}/AU_multi/MS2000R.component)
    add_custom_command(TARGET MS2000R_AUShim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MS2K_AU_MULTI}
        COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:MS2000R_VST_AU> ${MS2K_AU_MULTI}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MS2K_AU_MULTI}/Contents/Resources
        COMMAND ${CMAKE_COMMAND} -E rename ${MS2K_AU_MULTI}/Contents/MacOS/MS2000R ${MS2K_AU_MULTI}/Contents/Resources/MS2000R_engine
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MS2000R_AUShim> ${MS2K_AU_MULTI}/Contents/MacOS/MS2000R
        COMMAND xattr -cr ${MS2K_AU_MULTI}
        COMMAND codesign --force --sign - ${MS2K_AU_MULTI}/Contents/Resources/MS2000R_engine
        COMMAND codesign --force --sign - ${MS2K_AU_MULTI})

    # EIGHT-1 (2026-10-26): MS2000R 8-voice - the same plugin with a second machine behind the panel (companion*.cpp,
    # docs/eight_voice.md). Audio Unit only, in its own bundle with the same loader module:
    # build_vst/AU_multi/MS2000R8.component. AU_EXPORT_PREFIX keeps the factory name the loader looks for.
    juce_add_plugin(MS2000R8_VST
        COMPANY_NAME "MS2000 emulation project"
        PRODUCT_NAME "MS2000R8"
        PLUGIN_NAME "MS2000R 8-voice"
        PLUGIN_MANUFACTURER_CODE Msem
        PLUGIN_CODE Ms28
        AU_EXPORT_PREFIX MS2000RAU
        FORMATS AU
        BUNDLE_ID io.github.ms2000r-emu.plugin8
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)
    juce_generate_juce_header(MS2000R8_VST)
    target_sources(MS2000R8_VST PRIVATE PluginProcessor.cpp PluginEditor.cpp companion.cpp)
    # machine B's exports: nothing in the plugin calls them, so not in the shared-code archive (the linker would drop them)
    target_sources(MS2000R8_VST_AU PRIVATE companion_engine.cpp)
    target_include_directories(MS2000R8_VST PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/source)
    target_compile_definitions(MS2000R8_VST PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 MS2K_EIGHT_VOICE=1)
    target_link_libraries(MS2000R8_VST PRIVATE ms2000
        juce::juce_audio_utils juce::juce_audio_processors
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    add_library(MS2000R8_AUShim MODULE shim/ms2k_au_shim.cpp)
    target_compile_features(MS2000R8_AUShim PRIVATE cxx_std_17)
    set_target_properties(MS2000R8_AUShim PROPERTIES CXX_VISIBILITY_PRESET hidden PREFIX "" SUFFIX "" OUTPUT_NAME MS2000R8_au_shim)
    target_link_libraries(MS2000R8_AUShim PRIVATE "-framework AudioToolbox" "-framework CoreFoundation")
    # assembled on every build (not as the shim's POST_BUILD), so a rebuilt engine always reaches the bundle
    set(MS2K_AU8_MULTI ${CMAKE_BINARY_DIR}/AU_multi/MS2000R8.component)
    add_custom_target(MS2000R8_AU
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${MS2K_AU8_MULTI}
        COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:MS2000R8_VST_AU> ${MS2K_AU8_MULTI}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MS2K_AU8_MULTI}/Contents/Resources
        COMMAND ${CMAKE_COMMAND} -E rename ${MS2K_AU8_MULTI}/Contents/MacOS/MS2000R8 ${MS2K_AU8_MULTI}/Contents/Resources/MS2000R_engine
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MS2000R8_AUShim> ${MS2K_AU8_MULTI}/Contents/MacOS/MS2000R8
        COMMAND xattr -cr ${MS2K_AU8_MULTI}
        COMMAND codesign --force --sign - ${MS2K_AU8_MULTI}/Contents/Resources/MS2000R_engine
        COMMAND codesign --force --sign - ${MS2K_AU8_MULTI}
        COMMENT "Assembling AU_multi/MS2000R8.component")
    add_dependencies(MS2000R8_AU MS2000R8_AUShim MS2000R8_VST_AU)
endif()
