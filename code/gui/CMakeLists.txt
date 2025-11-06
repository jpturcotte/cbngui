cmake_minimum_required(VERSION 3.16)
project(CataclysmBN_GUI)

# GUI Manager Library
set(GUI_SOURCES
    gui_manager.cpp
    gui_renderer.cpp
)

set(GUI_HEADERS
    gui_manager.h
    gui_renderer.h
)

# Create static library for GUI module
add_library(cataclysm_gui STATIC ${GUI_SOURCES} ${GUI_HEADERS})

# Set C++ standard
target_compile_features(cataclysm_gui PUBLIC cxx_std_17)

# Set include directories
target_include_directories(cataclysm_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# Link dependencies
target_link_libraries(cataclysm_gui
    ${SDL2_LIBRARIES}
)

# In a real project, you would also link Dear ImGui libraries
# target_link_libraries(cataclysm_gui
#     imgui
#     imgui_sdl2_binding
#     imgui_sdlrenderer_binding
# )

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
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
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
    add_executable(gui_test test_gui_manager.cpp)
    target_link_libraries(gui_test cataclysm_gui)
    target_include_directories(gui_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    
    # Link required libraries for test
    target_link_libraries(gui_test
        ${SDL2_LIBRARIES}
    )
    
    add_test(NAME gui_manager_test COMMAND gui_test)
endif()