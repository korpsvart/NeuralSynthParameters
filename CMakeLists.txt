
cmake_minimum_required(VERSION 3.15)

project(JUCE_PLUGIN_WITH_TORCH VERSION 0.0.1)


### some libtorch stuff
# https://forum.juce.com/t/using-libtorch-with-juce/41154/4
# list(APPEND CMAKE_PREFIX_PATH "<my_path_to>/libtorch-win-shared-with-deps-debug-1.12.0+cpu/libtorch")
set(CMAKE_PREFIX_PATH "../../../source/libtorch-2/") # location of libtorch
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
### end libtorch stuff


add_subdirectory(../JUCE ./JUCE)                    # If you've put JUCE in a subdirectory called JUCE

juce_add_modules (./foleys_gui_magic ALIAS_NAMESPACE foleys)


juce_add_plugin(fm-torchknob
    # VERSION ...                               # Set this if the plugin version is different to the project version
    # ICON_BIG ...                              # ICON_* arguments specify a path to an image file to use as an icon for the Standalone
    # ICON_SMALL ...
    COMPANY_NAME Yeeking                          # Specify the name of the plugin's author
    DONT_SET_USING_JUCE_NAMESPACE TRUE
    # IS_SYNTH TRUE/FALSE                       # Is this a synth or an effect?
    # NEEDS_MIDI_INPUT TRUE/FALSE               # Does the plugin need midi input?
    # NEEDS_MIDI_OUTPUT TRUE/FALSE              # Does the plugin need midi output?
    # IS_MIDI_EFFECT TRUE/FALSE                 # Is this plugin a MIDI effect?
    # EDITOR_WANTS_KEYBOARD_FOCUS TRUE/FALSE    # Does the editor need keyboard focus?
    # COPY_PLUGIN_AFTER_BUILD TRUE/FALSE        # Should the plugin be installed to a default location after building?
    PLUGIN_MANUFACTURER_CODE Juce               # A four-character manufacturer id with at least one upper-case character
    PLUGIN_CODE Dem0                            # A unique four-character plugin id with exactly one upper-case character
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case
    FORMATS AU VST3 Standalone                  # The formats to build. Other valid formats are: AAX Unity VST AU AUv3
    PRODUCT_NAME "NeuralSynthParams")        # The name of the final executable, which can differ from the target name



juce_generate_juce_header(fm-torchknob)

target_sources(fm-torchknob
    PRIVATE
        src/PluginProcessor.cpp
	src/NeuralNetwork.h
	src/NeuralNetwork.cpp
	src/SynthVoice.h
	src/SynthVoice.cpp
	src/SynthSound.h)

add_executable(test_nn
	src/NeuralNetwork.cpp
	src/test_nn.cpp)
target_link_libraries(test_nn "${TORCH_LIBRARIES}")
set_property(TARGET test_nn PROPERTY CXX_STANDARD 14)


target_compile_definitions(fm-torchknob
    PUBLIC
        # switch the following off in the product to hide editor
        FOLEYS_SHOW_GUI_EDITOR_PALLETTE=1
        FOLEYS_SAVE_EDITED_GUI_IN_PLUGIN_STATE=0
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)


target_link_libraries(fm-torchknob
    PRIVATE
        # AudioPluginData           # If we'd created a binary data target, we'd link to it here
        juce::juce_audio_utils
        juce::juce_dsp
        foleys_gui_magic
        # add torch to the libraries to link against
        "${TORCH_LIBRARIES}"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


# The following code block is suggested to be used on Windows.
# According to https://github.com/pytorch/pytorch/issues/25457,
# the DLLs need to be copied to avoid memory errors.
if (MSVC)
#  set_property(TARGET fm-torchknob_Standalone
#
#              PROPERTY
#               MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


  file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
  add_custom_command(TARGET fm-torchknob_Standalone
                     POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     $<TARGET_FILE_DIR:fm-torchknob_Standalone>)
  add_custom_command(TARGET test_nn
                     POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     $<TARGET_FILE_DIR:test_nn>)
endif (MSVC)



