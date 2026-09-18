// ==============================================================
// main.cpp - プロジェクト一元管理くん バックエンド(C++)叩き台
//
// 【事前に必要な準備】
// 1. webview ライブラリ(https://github.com/webview/webview)を導入
//    - webview.h / webview.cc をプロジェクトに追加
//    - ビルドにはC++コンパイラ + WebView2 SDKが必要
// 2. JSON操作のため nlohmann/json を導入(ヘッダ1つだけで使えます)
//    - https://github.com/nlohmann/json → json.hpp を配置するだけでOK
//
// 【役割】
// このファイルは「機能ブロックごとの関数の置き場所」と「JSとの橋渡し
// (bind登録)」だけを用意した叩き台です。各関数の中身(実際のロジック)
// はTODOコメントの通り、ご自身で実装していく想定です。
// ==============================================================

#include "webview.h"
#include "json.hpp" // nlohmann/json

#include <windows.h>
#include <shobjidl.h> // IFileOpenDialog(フォルダ選択ダイアログ用)
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==============================================================
// データ構造
// ==============================================================

struct ProjectData {
    std::string id;
    std::string folder_path;
    std::string category_tag;     // "personal" or "business"
    std::string platform_tag;     // "desktop" / "web" / "mobile"
    bool is_internal_system;
    std::string description;
    std::string created_at;
};

// ProjectData ⇔ JSON の変換(nlohmann/jsonの機能)
// これがあると json(project) のように書くだけで変換できて便利です
void to_json(json& j, const ProjectData& p) {
    j = json{
        {"id", p.id},
        {"folderPath", p.folder_path},
        {"category", p.category_tag},
        {"platform", p.platform_tag},
        {"isInternalSystem", p.is_internal_system},
        {"description", p.description},
        {"createdAt", p.created_at}
    };
}

// ==============================================================
// ブロックA: フォルダ選択(D&Dの代替として、まずはダイアログ方式)
// ==============================================================

// TODO: 実装する
// Windows標準の IFileOpenDialog を使い、フォルダ選択ダイアログを開く。
// 選択されたフォルダの絶対パスを std::string で返す(キャンセル時は空文字)。
//
// 参考の骨組み:
//   1. CoCreateInstance で IFileOpenDialog を作成
//   2. SetOptions で FOS_PICKFOLDERS を指定(ファイルではなくフォルダを選ばせる)
//   3. Show() でダイアログを表示
//   4. GetResult() → GetDisplayName(SIGDN_FILESYSPATH) でパス取得
// std::wstring(ワイド文字列) → std::string(UTF-8) への変換。
// 日本語などマルチバイト文字を含むパスを正しく扱うために必要。
std::string wstring_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &result[0], size_needed, nullptr, nullptr);
    return result;
}

std::string browse_folder_dialog() {
    std::string selected_path = "";

    // COM初期化。すでに他所(webviewライブラリ側など)で初期化済みの場合は
    // RPC_E_CHANGED_MODE が返ってくることがあるが、その場合も致命的ではないので続行する。
    HRESULT hr_init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool need_uninit = SUCCEEDED(hr_init);

    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFileOpen));

    if (SUCCEEDED(hr)) {
        DWORD options = 0;
        hr = pFileOpen->GetOptions(&options);

        if (SUCCEEDED(hr)) {
            hr = pFileOpen->SetOptions(options | FOS_PICKFOLDERS);
        }

        if (SUCCEEDED(hr)) {
            hr = pFileOpen->Show(nullptr);

            // ユーザーがキャンセルした場合はエラー扱いにしない(単に未選択として返す)
            if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
                pFileOpen->Release();
                if (need_uninit) CoUninitialize();
                return "";
            }
        }

        if (SUCCEEDED(hr)) {
            IShellItem* pItem = nullptr;
            hr = pFileOpen->GetResult(&pItem);

            if (SUCCEEDED(hr)) {
                PWSTR pszFilePath = nullptr;
                hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

                if (SUCCEEDED(hr)) {
                    selected_path = wstring_to_utf8(std::wstring(pszFilePath));
                    CoTaskMemFree(pszFilePath);
                }
                pItem->Release(); // 取得したIShellItemは使い終わったら解放する
            }
        }
        pFileOpen->Release(); // ダイアログ自体も解放する
    }

    if (need_uninit) {
        CoUninitialize();
    }

    return selected_path;
}

// TODO: 実装する
// 指定フォルダ内のファイルを再帰的に走査し、パス一覧を返す。
// std::filesystem::recursive_directory_iterator が使えます。
std::vector<std::string> scan_folder_recursive(const std::string& folder_path) {
    std::vector<std::string> result;
    // TODO: ここに走査処理を書く
    return result;
}

// ==============================================================
// ブロックE: プロジェクトの保存・一覧取得
// ==============================================================

// TODO: 実装する
// ProjectData をファイル(例: JSON1ファイルにまとめて追記)またはDBへ保存する。
// まずは実行ファイルと同じ場所に "projects.json" を1ファイル作り、
// 配列として追記していく方式がシンプルでおすすめです。
void save_project_metadata(const ProjectData& project) {
    // TODO: ここに保存処理を書く
}

// TODO: 実装する
// 保存済みの全プロジェクトを読み込んで返す(ホーム画面の一覧表示用)。
std::vector<ProjectData> get_all_projects() {
    std::vector<ProjectData> result;
    // TODO: ここに読み込み処理を書く
    return result;
}

// ==============================================================
// ブロックF: プロジェクト詳細(ファイル一覧・プレビュー)
// ==============================================================

// TODO: 実装する
// 指定プロジェクトのファイル一覧を返す(ファイル名・拡張子・パス)。
std::vector<std::string> get_project_files(const std::string& project_id) {
    std::vector<std::string> result;
    // TODO: ここに実装を書く
    return result;
}

// TODO: 実装する
// 指定ファイルの中身をテキストとして読み込む(プレビュー・エディタ両方で使う)。
std::string read_file_content(const std::string& path) {
    // TODO: ここに実装を書く(std::ifstreamでの読み込み)
    return "";
}

// TODO: 実装する
// 指定ファイルへ内容を書き込む(保存処理)。
void write_file_content(const std::string& path, const std::string& content) {
    // TODO: ここに実装を書く(std::ofstreamでの書き込み)
}

// ==============================================================
// ブロックG: 外部アプリでファイルを開く + 変更監視の開始
// ==============================================================

// TODO: 実装する
// OS標準の関連付けアプリでファイルを開く。
// Windowsでは ShellExecute(NULL, "open", path.c_str(), NULL, NULL, SW_SHOW) が使えます。
void open_with_default_app(const std::string& path) {
    // TODO: ここに ShellExecute の実装を書く
}

// TODO: 実装する(難易度が高めのブロックです)
// 指定ファイルの変更を監視し始める。
// Windowsでは ReadDirectoryChangesW を使うのが標準的です。
// 別スレッドで監視ループを回し、変更を検知したら create_backup() を呼ぶ、
// という流れになります。
void start_watching_file(const std::string& path) {
    // TODO: ここに ReadDirectoryChangesW の実装を書く
}

// ==============================================================
// ブロックI: バックアップ保存
// ==============================================================

// TODO: 実装する
// 変更検知時点のファイル内容を、タイムスタンプ付きで履歴フォルダに退避する。
// 例: プロジェクトフォルダ内に ".history/style.css_20260918_1420.bak" のように保存
void create_backup(const std::string& path) {
    // TODO: ここに実装を書く
}

// TODO: 実装する
// 指定ファイルの過去バージョン(バックアップ)一覧を返す。
std::vector<std::string> get_file_history(const std::string& path) {
    std::vector<std::string> result;
    // TODO: ここに実装を書く(.historyフォルダの中を走査)
    return result;
}

// ==============================================================
// ブロックJ: 差分計算・復元
// ==============================================================

// TODO: 実装する
// 2つの内容を行単位で比較し、追加/削除された行を判定する。
// 最初はシンプルな実装(1行ずつ単純比較)から始めて、
// 慣れてきたらMyers差分アルゴリズムなどに発展させるのがおすすめです。
json compute_diff(const std::string& old_content, const std::string& new_content) {
    json diff_lines = json::array();
    // TODO: ここに差分計算を書く
    return diff_lines;
}

// TODO: 実装する
// 指定バックアップの内容を、現在のファイルへ上書きする(復元処理)。
void restore_backup(const std::string& path, const std::string& backup_id) {
    // TODO: ここに実装を書く
}

// ==============================================================
// main: webviewの起動とJSブリッジの登録
// ==============================================================

int main() {
    webview::webview w(true, nullptr);
    w.set_title("プロジェクト一元管理くん");
    w.set_size(1000, 720, WEBVIEW_HINT_NONE);

    // ---- ブロックA: フォルダ選択 ----
    // JS側: window.browseFolder().then(path => { ... })
    w.bind("browseFolder", [](const std::string& req) -> std::string {
        std::string path = browse_folder_dialog();
        return json(path).dump();
    });

    // ---- ブロックE: プロジェクト作成 ----
    // JS側: window.createProject(projectData)
    w.bind("createProject", [](const std::string& req) -> std::string {
        // reqは配列形式のJSON文字列("[ {...} ]")で届くので、
        // 先頭要素を取り出して使う
        json args = json::parse(req);
        json data = args[0];

        ProjectData project;
        project.folder_path = data.value("folderPath", "");
        project.category_tag = data.value("category", "");
        project.platform_tag = data.value("platform", "");
        project.is_internal_system = data.value("isInternalSystem", false);
        project.description = data.value("description", "");
        // TODO: idとcreated_atを生成する処理を追加する

        save_project_metadata(project);
        return json(true).dump();
    });

    // ---- ブロックE: プロジェクト一覧取得 ----
    // JS側: window.getProjects().then(projects => { ... })
    w.bind("getProjects", [](const std::string& req) -> std::string {
        std::vector<ProjectData> projects = get_all_projects();
        json result = projects; // to_jsonが定義済みなので自動変換される
        return result.dump();
    });

    // ---- ブロックG: 外部アプリで開く ----
    // JS側: window.openInExternalApp(filePath)
    w.bind("openInExternalApp", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string path = args[0];
        open_with_default_app(path);
        start_watching_file(path);
        return json(true).dump();
    });

    // TODO: 以下、必要に応じて同じパターンでbind登録を追加していく
    // - getProjectDetail (ブロックF)
    // - getFileDiff (ブロックJ)
    // - restoreBackup (ブロックJ)

    w.navigate("file://" + fs::absolute("index.html").string());
    w.run();

    return 0;
}
