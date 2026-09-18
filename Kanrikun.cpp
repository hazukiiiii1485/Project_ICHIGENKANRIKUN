// ==============================================================
// Kanrikun.cpp - プロジェクト一元管理くん バックエンド(C++)実装
// ==============================================================

#include "webview/webview.h"
#include "json.hpp" // nlohmann/json

#include <windows.h>
#include <shobjidl.h> // IFileOpenDialog

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ==============================================================
// 文字コード変換 / パス変換ヘルパー
//
// 重要: プロジェクトフォルダ名やファイル名に日本語が含まれるため、
// std::string(狭い文字列)は常にUTF-8として統一して扱う。
// ファイルシステム操作は fs::path(std::wstring) 経由で行うことで、
// Windows上でも日本語パスを正しく扱えるようにしている。
// ==============================================================

std::wstring utf8_to_wstring(const std::string& s) {
    if (s.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &result[0], size_needed);
    return result;
}

std::string wstring_to_utf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &result[0], size_needed, nullptr, nullptr);
    return result;
}

// UTF-8のstd::stringから、日本語パスも安全に扱えるfs::pathを作る
fs::path to_fs_path(const std::string& utf8_path) {
    return fs::path(utf8_to_wstring(utf8_path));
}

std::string generate_id() {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "proj_" + std::to_string(ms);
}

std::string current_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M");
    return oss.str();
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ==============================================================
// データ構造
// ==============================================================

struct ProjectData {
    std::string id;
    std::string folder_path;
    std::string category_tag;     // "personal" or "business"
    std::string platform_tag;     // "desktop" / "web" / "mobile"
    bool is_internal_system = false;
    std::string description;
    std::string created_at;
};

void to_json(json& j, const ProjectData& p) {
    j = json{
        {"id", p.id},
        {"folderPath", p.folder_path},
        {"category", p.category_tag},
        {"platform", p.platform_tag},
        {"isInternalSystem", p.is_internal_system},
        {"description", p.description},
        {"createdAt", p.created_at},
        {"name", to_fs_path(p.folder_path).filename().u8string()} // フォルダ名をそのままプロジェクト名として使う
    };
}

void from_json(const json& j, ProjectData& p) {
    p.id = j.value("id", "");
    p.folder_path = j.value("folderPath", "");
    p.category_tag = j.value("category", "");
    p.platform_tag = j.value("platform", "");
    p.is_internal_system = j.value("isInternalSystem", false);
    p.description = j.value("description", "");
    p.created_at = j.value("createdAt", "");
}

// ==============================================================
// ブロックA: フォルダ選択ダイアログ
// ==============================================================

std::string browse_folder_dialog() {
    std::string selected_path = "";

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
                pItem->Release();
            }
        }
        pFileOpen->Release();
    }

    if (need_uninit) CoUninitialize();
    return selected_path;
}

// 指定フォルダ内のファイルを再帰的に走査する。
// ".history" フォルダ(変更履歴の保存先)は一覧に出さないよう除外する。
std::vector<std::string> scan_folder_recursive(const std::string& folder_path) {
    std::vector<std::string> result;
    std::error_code ec;
    fs::path root = to_fs_path(folder_path);

    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {

        const fs::directory_entry& entry = *it;

        if (entry.path().filename() == L".history") {
            it.disable_recursion_pending(); // .history の中へは潜らない
            continue;
        }

        std::error_code file_ec;
        if (entry.is_regular_file(file_ec) && !file_ec) {
            result.push_back(wstring_to_utf8(entry.path().wstring()));
        }
    }
    return result;
}

// ==============================================================
// ブロックE: プロジェクトの保存・一覧取得
//
// projects.json という1ファイルに、プロジェクトの配列として保存する
// シンプルな方式。データ量が増えてきたら本格的なDBへの移行を検討する。
// ==============================================================

fs::path projects_json_path() {
    return fs::absolute("projects.json");
}

void save_project_metadata(const ProjectData& project) {
    json all = json::array();

    std::ifstream in(projects_json_path());
    if (in.is_open()) {
        try {
            in >> all;
        } catch (...) {
            all = json::array(); // 壊れていた/空だった場合は空配列から始める
        }
        in.close();
    }

    all.push_back(project);

    std::ofstream out(projects_json_path());
    out << all.dump(2);
}

std::vector<ProjectData> get_all_projects() {
    std::vector<ProjectData> result;

    std::ifstream in(projects_json_path());
    if (!in.is_open()) return result; // まだ1件も作られていない場合

    json all;
    try {
        in >> all;
    } catch (...) {
        return result;
    }

    for (const auto& item : all) {
        result.push_back(item.get<ProjectData>());
    }
    return result;
}

bool find_project_by_id(const std::string& project_id, ProjectData& out) {
    for (const auto& p : get_all_projects()) {
        if (p.id == project_id) {
            out = p;
            return true;
        }
    }
    return false;
}

// ==============================================================
// ブロックF: プロジェクト詳細(ファイル一覧・読み込み・保存)
// ==============================================================

std::string read_file_content(const std::string& path) {
    std::ifstream file(to_fs_path(path), std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void write_file_content(const std::string& path, const std::string& content) {
    std::ofstream file(to_fs_path(path), std::ios::binary | std::ios::trunc);
    file << content;
}

// ==============================================================
// ブロックG: 外部アプリで開く + 変更監視
// ==============================================================

void open_with_default_app(const std::string& path) {
    ShellExecuteW(nullptr, L"open", to_fs_path(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ---- ファイル監視(ポーリング方式) ----
//
// 本来はWindows APIの ReadDirectoryChangesW を使うのがより高度な方式だが、
// 実装・保守のしやすさを優先し、まずは「1秒おきに更新日時を確認する」
// シンプルなポーリング方式にしている。1ファイルにつき1スレッドを立てる。

struct FileWatcher {
    std::atomic<bool> stop_flag{false};
    std::thread th;
};

std::map<std::string, std::unique_ptr<FileWatcher>> g_watchers;
std::mutex g_watchers_mutex;

void create_backup(const std::string& path, const std::string& content_to_save) {
    fs::path original = to_fs_path(path);
    fs::path history_dir = original.parent_path() / L".history";

    std::error_code ec;
    fs::create_directories(history_dir, ec);

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &t);
    std::wostringstream name;
    name << original.filename().wstring() << L"."
         << std::put_time(&tm_buf, L"%Y%m%d_%H%M%S") << L".bak";

    fs::path backup_path = history_dir / name.str();
    std::ofstream out(backup_path, std::ios::binary | std::ios::trunc);
    out << content_to_save;
}

void watch_file_loop(std::string path, std::atomic<bool>* stop_flag) {
    std::string last_content = read_file_content(path);

    while (!stop_flag->load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (stop_flag->load()) break;

        std::string current_content = read_file_content(path);
        if (current_content != last_content) {
            // 変更が検知された: 変更前の内容をバックアップとして退避する
            create_backup(path, last_content);
            last_content = current_content;
        }
    }
}

void start_watching_file(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_watchers_mutex);

    // 同じファイルを既に監視中なら、一旦止めてから新しく始める(二重起動防止)
    auto existing = g_watchers.find(path);
    if (existing != g_watchers.end()) {
        existing->second->stop_flag.store(true);
        if (existing->second->th.joinable()) existing->second->th.detach();
        g_watchers.erase(existing);
    }

    auto watcher = std::make_unique<FileWatcher>();
    std::atomic<bool>* stop_flag_ptr = &watcher->stop_flag;
    watcher->th = std::thread(watch_file_loop, path, stop_flag_ptr);
    watcher->th.detach(); // アプリ終了までバックグラウンドで動かし続ける

    g_watchers[path] = std::move(watcher);
}

// ==============================================================
// ブロックI: 変更履歴一覧
// ==============================================================

json get_file_history(const std::string& path) {
    json result = json::array();

    fs::path original = to_fs_path(path);
    fs::path history_dir = original.parent_path() / L".history";
    std::wstring prefix = original.filename().wstring() + L".";

    std::error_code ec;
    if (!fs::exists(history_dir, ec)) return result;

    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(history_dir, ec)) {
        if (entry.path().filename().wstring().rfind(prefix, 0) == 0) {
            entries.push_back(entry);
        }
    }

    // 新しい順に並べる
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        std::error_code e1, e2;
        return fs::last_write_time(a, e1) > fs::last_write_time(b, e2);
    });

    for (const auto& entry : entries) {
        result.push_back({
            {"backupId", wstring_to_utf8(entry.path().filename().wstring())},
            {"path", wstring_to_utf8(entry.path().wstring())}
        });
    }
    return result;
}

// ==============================================================
// ブロックJ: 差分計算・復元
// ==============================================================

// LCS(最長共通部分列)を使った行単位の差分計算。
// ファイルが非常に大きい場合は計算量が増えるため、通常のソースコード
// ファイル程度のサイズを想定している。
json compute_diff(const std::string& old_content, const std::string& new_content) {
    std::vector<std::string> old_lines = split_lines(old_content);
    std::vector<std::string> new_lines = split_lines(new_content);

    size_t n = old_lines.size(), m = new_lines.size();
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));

    for (size_t i = 1; i <= n; i++) {
        for (size_t j = 1; j <= m; j++) {
            if (old_lines[i - 1] == new_lines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    std::vector<json> result;
    size_t i = n, j = m;
    while (i > 0 && j > 0) {
        if (old_lines[i - 1] == new_lines[j - 1]) {
            result.push_back({{"type", "same"}, {"text", old_lines[i - 1]}});
            i--; j--;
        } else if (dp[i - 1][j] >= dp[i][j - 1]) {
            result.push_back({{"type", "remove"}, {"text", old_lines[i - 1]}});
            i--;
        } else {
            result.push_back({{"type", "add"}, {"text", new_lines[j - 1]}});
            j--;
        }
    }
    while (i > 0) { result.push_back({{"type", "remove"}, {"text", old_lines[i - 1]}}); i--; }
    while (j > 0) { result.push_back({{"type", "add"}, {"text", new_lines[j - 1]}}); j--; }

    std::reverse(result.begin(), result.end());
    return json(result);
}

void restore_backup(const std::string& path, const std::string& backup_id) {
    fs::path original = to_fs_path(path);
    fs::path history_dir = original.parent_path() / L".history";
    fs::path backup_path = history_dir / utf8_to_wstring(backup_id);

    std::string backup_content = read_file_content(wstring_to_utf8(backup_path.wstring()));

    // 復元前の現在の内容も念のため退避しておく(復元も取り消せるように)
    std::string current_content = read_file_content(path);
    create_backup(path, current_content);

    write_file_content(path, backup_content);
}

// ==============================================================
// main: webviewの起動とJSブリッジの登録
// ==============================================================

int main() {
    webview::webview w(true, nullptr);
    w.set_title(u8"プロジェクト一元管理くん");
    w.set_size(1000, 720, WEBVIEW_HINT_NONE);

    // ---- ブロックA: フォルダ選択 ----
    w.bind("browseFolder", [](const std::string&) -> std::string {
        return json(browse_folder_dialog()).dump();
    });

    // ---- ブロックE: プロジェクト作成 ----
    w.bind("createProject", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        json data = args[0];

        ProjectData project;
        project.id = generate_id();
        project.folder_path = data.value("folderPath", "");
        project.category_tag = data.value("category", "");
        project.platform_tag = data.value("platform", "");
        project.is_internal_system = data.value("isInternalSystem", false);
        project.description = data.value("description", "");
        project.created_at = current_timestamp_string();

        save_project_metadata(project);
        return json(true).dump();
    });

    // ---- ブロックE: プロジェクト一覧取得 ----
    w.bind("getProjects", [](const std::string&) -> std::string {
        json result = get_all_projects();
        return result.dump();
    });

    // ---- ブロックF: プロジェクト詳細(ファイル一覧)取得 ----
    w.bind("getProjectDetail", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string project_id = args[0];

        ProjectData project;
        if (!find_project_by_id(project_id, project)) {
            return json(nullptr).dump();
        }

        std::vector<std::string> files = scan_folder_recursive(project.folder_path);
        json files_json = json::array();
        for (const auto& file_path : files) {
            files_json.push_back({
                {"name", to_fs_path(file_path).filename().u8string()},
                {"path", file_path}
            });
        }

        json result = project;
        result["files"] = files_json;
        return result.dump();
    });

    // ---- ファイルの中身を取得(プレビュー用) ----
    w.bind("readFile", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string path = args[0];
        return json(read_file_content(path)).dump();
    });

    // ---- ブロックG: 外部アプリで開く + 監視開始 ----
    w.bind("openInExternalApp", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string path = args[0];
        open_with_default_app(path);
        start_watching_file(path);
        return json(true).dump();
    });

    // ---- ブロックI: 変更履歴一覧 ----
    w.bind("getFileHistory", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string path = args[0];
        return get_file_history(path).dump();
    });

    // ---- ブロックJ: 差分取得(現在の内容 or 別バックアップ と、指定バックアップを比較) ----
    w.bind("getFileDiff", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string path = args[0];
        std::string backup_id = args[1];

        fs::path original = to_fs_path(path);
        fs::path backup_path = original.parent_path() / L".history" / utf8_to_wstring(backup_id);

        std::string backup_content = read_file_content(wstring_to_utf8(backup_path.wstring()));
        std::string current_content = read_file_content(path);

        json result;
        result["diff"] = compute_diff(backup_content, current_content);
        return result.dump();
    });

    // ---- ブロックJ: 復元 ----
    w.bind("restoreBackup", [](const std::string& req) -> std::string {
        json args = json::parse(req);
        std::string path = args[0];
        std::string backup_id = args[1];
        restore_backup(path, backup_id);
        return json(true).dump();
    });

    w.navigate("file://" + wstring_to_utf8(fs::absolute("index.html").wstring()));
    w.run();

    return 0;
}
