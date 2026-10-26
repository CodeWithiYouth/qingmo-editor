cmake_minimum_required(VERSION 3.25)

project(QingMo
    VERSION 0.1.0
    DESCRIPTION "A lightweight text editor built on the SciTE/Scintilla skeleton"
    LANGUAGES C CXX RC)

set(APP_NAME "QingMo" CACHE STRING "Executable / product display name")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "This skeleton currently targets Windows + MSVC only.")
endif()

set(TP "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(APP_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Keep the upstream warning profile, but do not let third-party warnings stop us.
add_compile_options(/utf-8 /W4 /wd4702 /wd4324)

# ---------------------------------------------------------------------------
# Scintilla (editing engine)
# ---------------------------------------------------------------------------
file(GLOB SCINTILLA_CORE CONFIGURE_DEPENDS "${TP}/scintilla/src/*.cxx")
file(GLOB SCINTILLA_WIN  CONFIGURE_DEPENDS "${TP}/scintilla/win32/*.cxx")

add_library(scintilla_obj OBJECT
    ${SCINTILLA_CORE}
    ${SCINTILLA_WIN}
    "${TP}/scintilla/call/ScintillaCall.cxx"
)
target_include_directories(scintilla_obj PUBLIC
    "${TP}/scintilla/include"
    "${TP}/scintilla/src"
    "${TP}/scintilla/win32"
    "${TP}/scintilla/call"
    "${TP}/lexilla/include"
)
target_compile_definitions(scintilla_obj PRIVATE
    WIN32 _WINDOWS UNICODE _UNICODE STATIC_BUILD
    _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE _SCL_SECURE_NO_WARNINGS
)

# ---------------------------------------------------------------------------
# Lexilla (syntax lexers)
# ---------------------------------------------------------------------------
file(GLOB LEXILLA_LEXLIB CONFIGURE_DEPENDS "${TP}/lexilla/lexlib/*.cxx")
file(GLOB LEXILLA_LEXERS CONFIGURE_DEPENDS "${TP}/lexilla/lexers/*.cxx")

add_library(lexilla_obj OBJECT
    "${TP}/lexilla/src/Lexilla.cxx"
    "${TP}/lexilla/access/LexillaAccess.cxx"
    ${LEXILLA_LEXLIB}
    ${LEXILLA_LEXERS}
)
target_include_directories(lexilla_obj PUBLIC
    "${TP}/lexilla/include"
    "${TP}/lexilla/lexlib"
    "${TP}/lexilla/access"
    "${TP}/scintilla/include"
)
target_compile_definitions(lexilla_obj PRIVATE
    WIN32 _WINDOWS UNICODE _UNICODE STATIC_BUILD
    _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE _SCL_SECURE_NO_WARNINGS
)

# ---------------------------------------------------------------------------
# Lua 5.3 (SciTE scripting host)
# ---------------------------------------------------------------------------
file(GLOB LUA_SOURCES CONFIGURE_DEPENDS "${TP}/lua/src/*.c")

add_library(lua_obj OBJECT ${LUA_SOURCES})
target_include_directories(lua_obj PUBLIC
    "${TP}/lua/src"
    "${APP_INCLUDE}"
)
target_compile_definitions(lua_obj PRIVATE
    "LUA_USER_H=\"scite_lua_win.h\""
    _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE _SCL_SECURE_NO_WARNINGS
)

# ---------------------------------------------------------------------------
# Application (SciTE shell)
# ---------------------------------------------------------------------------
file(GLOB APP_CORE CONFIGURE_DEPENDS "${APP_INCLUDE}/*.cxx")
file(GLOB APP_WIN  CONFIGURE_DEPENDS "${APP_INCLUDE}/win32/*.cxx")

add_executable(qingmo WIN32
    ${APP_CORE}
    ${APP_WIN}
    "${APP_INCLUDE}/win32/SciTERes.rc"
)

set_target_properties(qingmo PROPERTIES
    OUTPUT_NAME "${APP_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(qingmo PRIVATE
    "${APP_INCLUDE}"
    "${APP_INCLUDE}/win32"
)
target_compile_definitions(qingmo PRIVATE
    WIN32 _WINDOWS UNICODE _UNICODE STATIC_BUILD
    _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE _SCL_SECURE_NO_WARNINGS
    "LUA_USER_H=\"scite_lua_win.h\""
)
target_link_libraries(qingmo PRIVATE
    scintilla_obj
    lexilla_obj
    lua_obj
    gdi32 comdlg32 comctl32 advapi32 imm32 shell32 ole32 oleaut32 uxtheme user32
)
target_link_options(qingmo PRIVATE
    # SciTERes.rc embeds src/win32/QingMo.exe.manifest as RT_MANIFEST.
    # Disable the linker-generated manifest so it is not embedded twice.
    /MANIFEST:NO
    $<$<CONFIG:Release>:/OPT:ICF>
    $<$<CONFIG:RelWithDebInfo>:/OPT:ICF>
    $<$<CONFIG:MinSizeRel>:/OPT:ICF>
)

# SciTE reads its properties from the executable directory. Keep it portable.
add_custom_command(TARGET qingmo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:qingmo>"
    COMMENT "Copying SciTE configuration"
    VERBATIM
)

# ---------------------------------------------------------------------------
# Convenience target: build and stage a distributable folder
# ---------------------------------------------------------------------------
add_custom_target(package
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/package"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:qingmo>"
        "${CMAKE_BINARY_DIR}/package/"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "${CMAKE_BINARY_DIR}/package"
    DEPENDS qingmo
    COMMENT "Staging ${APP_NAME} into build/package"
    VERBATIM
)
