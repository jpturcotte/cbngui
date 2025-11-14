cmake_minimum_required(VERSION 3.16)
project(CataclysmBN_GUI)

option(BUILD_TESTING "Build the tests" ON)

if(BUILD_TESTING)
    enable_testing()
endif()

find_package(SDL2 REQUIRED)

# GUI Manager Library
set(GUI_SOURCES
    overlay_manager.cpp
    overlay_input_adapter.cpp
    overlay_renderer.cpp
    overlay_ui.cpp
    ui_adaptor.cpp
    ui_manager.cpp
    map_widget.cpp
    InventoryWidget.cpp
    CharacterWidget.cpp
    input_manager.cpp
    event_bus.cpp
    event_bus_adapter.cpp
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/backends/imgui_impl_sdl2.cpp
    libs/imgui/backends/imgui_impl_sdlrenderer2.cpp
)

set(GUI_HEADERS
    overlay_manager.h
    overlay_input_adapter.h
    overlay_renderer.h
    overlay_ui.h
    ui_adaptor.h
    ui_manager.h
    map_widget.h
    InventoryWidget.h
    InventoryOverlayState.h
    CharacterWidget.h
    CharacterOverlayState.h
    input_manager.h
    debug.h
    json.h
    event_bus.h
    event_bus_adapter.h
    mock_events.h
)

# Create static library for GUI module
add_library(cataclysm_gui STATIC ${GUI_SOURCES} ${GUI_HEADERS})

# Set C++ standard
target_compile_features(cataclysm_gui PUBLIC cxx_std_17)

# Set include directories
target_include_directories(cataclysm_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
    libs/imgui
    libs/imgui/backends
)

# Link dependencies
target_link_libraries(cataclysm_gui
    SDL2::SDL2
)

# Set compiler-specific flags
if(MSVC)
    target_compile_options(cataclysm_gui PRIVATE /W4 /WX)
else()
    target_compile_options(cataclysm_gui PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Add compile definitions for different build types
target_compile_definitions(cataclysm_gui PUBLIC
    GUI_MANAGER_EXPORTS
)

# Set library properties
set_target_properties(cataclysm_gui PROPERTIES
    OUTPUT_NAME "cataclysm_gui"
)

# Install targets
install(TARGETS cataclysm_gui
    EXPORT CataclysmBNTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

# Install headers
install(FILES ${GUI_HEADERS}
    DESTINATION include/cataclysm_gui
)

# Export the library
if(BUILD_TESTING)
    # Add test target (in a full implementation)
    add_executable(test_gui)
    target_sources(test_gui PRIVATE
        test_gui.cpp
        "libs/imgui/imgui_demo.cpp"
    )
    target_link_libraries(test_gui cataclysm_gui SDL2::SDL2)
    target_include_directories(test_gui PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SDL2_INCLUDE_DIRS}
        libs/imgui
        libs/imgui/backends
    )
    
    add_test(NAME gui_manager_test COMMAND test_gui)
endif()
