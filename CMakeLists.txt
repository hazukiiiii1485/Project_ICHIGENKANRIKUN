cmake_minimum_required(VERSION 3.10.0)
project(kanrikun VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kanrikun Kanrikun.cpp)

# --- webview/webview.h およびその仲間のファイル一式の場所 ---
# 例: include/webview/webview.h, include/webview/api.h ... という構成を想定
target_include_directories(kanrikun PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# --- json.hpp(プロジェクト直下にある場合) ---
target_include_directories(kanrikun PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# --- WebView2 SDK のヘッダーとlibの場所(ご自身の配置に合わせて調整してください) ---
target_include_directories(kanrikun PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_directories(kanrikun PRIVATE
    ${CMAKE_SOURCE_DIR}/x64
)

# --- 必要なライブラリのリンク ---
target_link_libraries(kanrikun PRIVATE
    WebView2Loader   # WebView2 SDK
    ole32            # IFileOpenDialogなどCOM関連
    shell32          # ShellExecuteなど
    user32
)

# ビルド後、WebView2Loader.dll を .exe と同じ場所へ自動コピーする
add_custom_command(TARGET kanrikun POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/WebView2Loader.dll
        $<TARGET_FILE_DIR:kanrikun>
)

# ビルド後、UI一式(index.html / style.css / app.js)も .exe と同じ場所へコピーする
add_custom_command(TARGET kanrikun POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/index.html
        ${CMAKE_SOURCE_DIR}/style.css
        ${CMAKE_SOURCE_DIR}/app.js
        $<TARGET_FILE_DIR:kanrikun>
)
