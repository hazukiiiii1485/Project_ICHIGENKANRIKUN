/* ==========================================================
   画面切り替え(ブロックE等で使用: SPA的にセクションの表示/非表示を切替)
   ========================================================== */
function showView(viewId) {
  document.querySelectorAll(".view").forEach(function (el) {
    el.classList.remove("is-active");
  });
  document.getElementById(viewId).classList.add("is-active");
}

function goToCreateScreen() {
  showView("view-create");
}

function goToHomeScreen() {
  showView("view-home");
  loadProjects();
}

/* ==========================================================
   ブロックA〜E: ホーム画面 プロジェクト一覧の取得と描画
   ========================================================== */

// C++側の getProjects をブリッジ経由で呼び出す。
// ビルドしたアプリ上でなければ window.getProjects は存在しないため、
// その場合はUI確認用の仮データにフォールバックする。
function fetchProjectsFromBackend() {
  if (typeof window.getProjects === "function") {
    return window.getProjects().then(function (projects) {
      // C++側は updatedAt を返さないので、createdAt を表示用に補う
      return projects.map(function (p) {
        return Object.assign({}, p, {
          updatedAt: p.createdAt ? `${p.createdAt}更新` : ""
        });
      });
    });
  }

  console.warn("getProjects が見つかりません。仮データを表示します(通常のブラウザで確認中と思われます)");
  return Promise.resolve([
    {
      id: "proj_001",
      name: "在庫管理システム",
      description: "倉庫の入出庫を社内スタッフ向けに一元化",
      category: "business",
      platform: "desktop",
      isInternalSystem: true,
      updatedAt: "09/10更新"
    },
    {
      id: "proj_002",
      name: "案件進捗ダッシュボード",
      description: "受託案件の進捗を取引先と共有",
      category: "business",
      platform: "web",
      isInternalSystem: false,
      updatedAt: "09/05更新"
    }
  ]);
}

const CATEGORY_ICON = {
  personal: "ti-user",
  business: "ti-briefcase"
};

const PLATFORM_LABEL = {
  desktop: { icon: "ti-device-desktop", text: "デスクトップ" },
  web: { icon: "ti-world", text: "Web" },
  mobile: { icon: "ti-device-mobile", text: "モバイル" }
};

function buildProjectCard(project) {
  const platform = PLATFORM_LABEL[project.platform];

  let tagsHtml = "";
  tagsHtml += `<span class="tag"><i class="ti ${platform.icon}" aria-hidden="true"></i>${platform.text}</span>`;
  if (project.isInternalSystem) {
    tagsHtml += `<span class="tag"><i class="ti ti-building" aria-hidden="true"></i>自社</span>`;
  }

  const card = document.createElement("div");
  card.style.cssText =
    "background:var(--color-surface); border:1px solid var(--main-15); border-radius:var(--radius-lg); padding:14px; cursor:pointer;";
  card.innerHTML = `
    <i class="ti ${CATEGORY_ICON[project.category]}" style="font-size:22px;" aria-hidden="true"></i>
    <div style="font-weight:500; font-size:14px; margin-top:10px;">${project.name}</div>
    <div style="font-size:12px; color:var(--main-60); margin-top:4px; line-height:1.4;">${project.description}</div>
    <div style="display:flex; flex-wrap:wrap; gap:4px; margin-top:10px;">${tagsHtml}</div>
    <div style="font-family:var(--font-mono); font-size:10px; color:var(--main-40); margin-top:10px;">${project.updatedAt}</div>
  `;
  card.addEventListener("click", function () {
    goToDetailScreen(project.id);
  });
  return card;
}

function buildNewProjectCard() {
  const card = document.createElement("div");
  card.style.cssText =
    "background:transparent; border:1px dashed var(--main-40); border-radius:var(--radius-lg); padding:14px; cursor:pointer; display:flex; flex-direction:column; align-items:center; justify-content:center; min-height:120px;";
  card.innerHTML = `
    <i class="ti ti-plus" style="font-size:20px; color:var(--main-60);" aria-hidden="true"></i>
    <div style="font-size:12px; color:var(--main-60); margin-top:8px;">新規プロジェクト</div>
  `;
  card.addEventListener("click", goToCreateScreen);
  return card;
}

function renderProjectCards(projects) {
  const grid = document.getElementById("project-grid");
  grid.innerHTML = "";
  projects.forEach(function (project) {
    grid.appendChild(buildProjectCard(project));
  });
  grid.appendChild(buildNewProjectCard());

  document.getElementById("project-count").textContent =
    `保存プロジェクト(全${projects.length}件)`;
}

function loadProjects() {
  fetchProjectsFromBackend().then(renderProjectCards);
}

/* ==========================================================
   ブロックF: プロジェクト詳細画面(ファイル一覧・プレビュー)
   ========================================================== */

let currentProjectId = null;
let currentProjectFiles = []; // [{name, path}, ...]

const FILE_ICON_EXT = {
  html: "ti-file-type-html",
  htm: "ti-file-type-html",
  css: "ti-file-type-css",
  js: "ti-file-code",
  cpp: "ti-file-code",
  h: "ti-file-code",
  json: "ti-file-code"
};

function getFileExtension(fileName) {
  const parts = fileName.split(".");
  return parts.length > 1 ? parts.pop().toLowerCase() : "";
}

function goToDetailScreen(projectId) {
  if (typeof window.getProjectDetail !== "function") {
    console.warn("getProjectDetail が見つかりません。ビルドしたアプリ上で確認してください。");
    return;
  }

  currentProjectId = projectId;
  showView("view-detail");

  window.getProjectDetail(projectId).then(function (project) {
    if (!project) {
      console.error("プロジェクトが見つかりませんでした:", projectId);
      return;
    }

    currentProjectFiles = project.files || [];

    document.getElementById("detail-project-name").textContent = project.name;

    const platform = PLATFORM_LABEL[project.platform];
    let tagsHtml = "";
    if (platform) {
      tagsHtml += `<span class="tag"><i class="ti ${platform.icon}" aria-hidden="true"></i>${platform.text}</span>`;
    }
    if (project.isInternalSystem) {
      tagsHtml += `<span class="tag"><i class="ti ti-building" aria-hidden="true"></i>自社システム</span>`;
    }
    document.getElementById("detail-tags").innerHTML = tagsHtml;

    renderFileList(currentProjectFiles);
    renderPreviewTabs(currentProjectFiles);
  });
}

function renderFileList(files) {
  const list = document.getElementById("file-list");
  list.innerHTML = "";

  files.forEach(function (file) {
    const ext = getFileExtension(file.name);
    const iconClass = FILE_ICON_EXT[ext] || "ti-file";

    const row = document.createElement("div");
    row.className = "file-row";
    row.innerHTML = `
      <i class="ti ${iconClass}" aria-hidden="true"></i>
      <span class="file-name">${file.name}</span>
      <i class="ti ti-history file-action" title="変更履歴" aria-hidden="true"></i>
      <i class="ti ti-chevron-right" style="opacity:0.4;" aria-hidden="true"></i>
    `;

    // ファイル名クリック → 外部アプリで開く
    row.querySelector(".file-name").addEventListener("click", function () {
      onFileRowClick(file.path);
    });

    // 履歴アイコンクリック → 変更履歴画面へ
    row.querySelector(".file-action").addEventListener("click", function (e) {
      e.stopPropagation();
      goToHistoryScreen(file);
    });

    list.appendChild(row);
  });
}

// ブロックG: 外部アプリで開く(メモ帳/VSCode等)+ 変更監視の開始
function onFileRowClick(filePath) {
  if (typeof window.openInExternalApp !== "function") {
    console.warn("openInExternalApp が見つかりません。ビルドしたアプリ上で確認してください。");
    return;
  }
  window.openInExternalApp(filePath);
}

/* ---- プレビュー(HTML/CSSをタブ切り替えで表示) ---- */

function renderPreviewTabs(files) {
  // まずはHTML/CSSファイルのうち、それぞれ最初の1つだけをプレビュー対象にする
  const previewTargets = [];
  const htmlFile = files.find(function (f) { return getFileExtension(f.name) === "html"; });
  const cssFile = files.find(function (f) { return getFileExtension(f.name) === "css"; });
  if (htmlFile) previewTargets.push({ label: "HTML", file: htmlFile });
  if (cssFile) previewTargets.push({ label: "CSS", file: cssFile });

  const tabsEl = document.getElementById("preview-tabs");
  tabsEl.innerHTML = "";

  if (previewTargets.length === 0) {
    document.getElementById("preview-content").textContent = "プレビュー可能なファイル(HTML/CSS)が見つかりません";
    return;
  }

  previewTargets.forEach(function (target, index) {
    const tab = document.createElement("button");
    tab.type = "button";
    tab.className = "preview-tab" + (index === 0 ? " is-selected" : "");
    tab.textContent = target.label;
    tab.addEventListener("click", function () {
      tabsEl.querySelectorAll(".preview-tab").forEach(function (t) {
        t.classList.remove("is-selected");
      });
      tab.classList.add("is-selected");
      loadPreviewContent(target.file.path);
    });
    tabsEl.appendChild(tab);
  });

  loadPreviewContent(previewTargets[0].file.path);
}

function loadPreviewContent(filePath) {
  const box = document.getElementById("preview-content");
  if (typeof window.readFile !== "function") {
    box.textContent = "(readFile が見つかりません。ビルドしたアプリ上で確認してください)";
    return;
  }
  window.readFile(filePath).then(function (content) {
    box.textContent = content || "(空のファイルです)";
  });
}

/* ==========================================================
   ブロックI・J: 変更履歴・差分・復元画面
   ========================================================== */

let currentHistoryFile = null; // {name, path}
let currentSelectedBackupId = null;

function goToHistoryScreen(file) {
  if (typeof window.getFileHistory !== "function") {
    console.warn("getFileHistory が見つかりません。ビルドしたアプリ上で確認してください。");
    return;
  }

  currentHistoryFile = file;
  currentSelectedBackupId = null;
  showView("view-history");

  document.getElementById("history-file-name").textContent = file.name;

  window.getFileHistory(file.path).then(function (backups) {
    renderHistoryList(backups);
  });
}

function renderHistoryList(backups) {
  const list = document.getElementById("history-list");
  list.innerHTML = "";

  // 「現在の内容」は常に一番上に表示する特別な項目(バックアップではない)
  const currentItem = document.createElement("div");
  currentItem.className = "history-item is-selected";
  currentItem.innerHTML = `
    <div style="font-size:12px; font-weight:500;">現在の内容</div>
    <div class="history-time">最新</div>
  `;
  currentItem.addEventListener("click", function () {
    selectHistoryItem(currentItem, null);
  });
  list.appendChild(currentItem);

  backups.forEach(function (backup) {
    const item = document.createElement("div");
    item.className = "history-item";
    item.innerHTML = `
      <div style="font-size:12px;">バックアップ</div>
      <div class="history-time">${backup.backupId}</div>
    `;
    item.addEventListener("click", function () {
      selectHistoryItem(item, backup.backupId);
    });
    list.appendChild(item);
  });

  // 差分表示エリアは初期状態(現在の内容のみ選択)にしておく
  document.getElementById("diff-view").innerHTML =
    '<div style="padding:12px; color:var(--main-60); font-size:12px;">過去のバージョンを選択すると、現在との差分が表示されます</div>';
  document.getElementById("restore-btn").disabled = true;
}

function selectHistoryItem(itemEl, backupId) {
  document.querySelectorAll(".history-item").forEach(function (el) {
    el.classList.remove("is-selected");
  });
  itemEl.classList.add("is-selected");
  currentSelectedBackupId = backupId;

  const restoreBtn = document.getElementById("restore-btn");

  if (!backupId) {
    // 「現在の内容」を選択した場合は差分なし
    document.getElementById("diff-view").innerHTML =
      '<div style="padding:12px; color:var(--main-60); font-size:12px;">過去のバージョンを選択すると、現在との差分が表示されます</div>';
    restoreBtn.disabled = true;
    return;
  }

  restoreBtn.disabled = false;

  if (typeof window.getFileDiff !== "function") {
    console.warn("getFileDiff が見つかりません。ビルドしたアプリ上で確認してください。");
    return;
  }

  window.getFileDiff(currentHistoryFile.path, backupId).then(function (result) {
    renderDiffView(result.diff || []);
  });
}

function renderDiffView(diffLines) {
  const view = document.getElementById("diff-view");
  view.innerHTML = "";

  diffLines.forEach(function (line) {
    const div = document.createElement("div");
    const prefix = line.type === "add" ? "+ " : line.type === "remove" ? "- " : "  ";
    div.className = "diff-line" + (line.type === "add" || line.type === "remove" ? " " + line.type : "");
    div.textContent = prefix + line.text;
    view.appendChild(div);
  });

  if (diffLines.length === 0) {
    view.innerHTML = '<div style="padding:12px; color:var(--main-60); font-size:12px;">差分はありません</div>';
  }
}

document.getElementById("restore-btn").addEventListener("click", function () {
  if (!currentHistoryFile || !currentSelectedBackupId) return;

  if (typeof window.restoreBackup !== "function") {
    console.warn("restoreBackup が見つかりません。ビルドしたアプリ上で確認してください。");
    return;
  }

  if (!confirm("このバージョンに戻します。現在の内容は上書きされます(復元前の内容も履歴に残ります)。よろしいですか?")) {
    return;
  }

  window.restoreBackup(currentHistoryFile.path, currentSelectedBackupId).then(function () {
    // 復元後、履歴一覧を最新の状態に更新する
    window.getFileHistory(currentHistoryFile.path).then(renderHistoryList);
  });
});

/* ==========================================================
   ブロックA: フォルダD&D欄
   ========================================================== */

// 選択されたフォルダの情報を保持する(C++側の実装が決まり次第、
// 実際に使う形へ調整してください)
let selectedFolder = null;

const dropZone = document.getElementById("drop-zone");
const dropZonePath = document.getElementById("drop-zone-path");

dropZone.addEventListener("dragover", function (e) {
  e.preventDefault(); // これがないとdropが発火しない
  dropZone.classList.add("is-dragover");
});

dropZone.addEventListener("dragleave", function () {
  dropZone.classList.remove("is-dragover");
});

dropZone.addEventListener("drop", function (e) {
  e.preventDefault();
  dropZone.classList.remove("is-dragover");

  const files = e.dataTransfer.files;
  console.log("ドロップされた内容:", files);

  // TODO: ここでフォルダパスの取得方法を確認し、selectedFolder に格納する。
  // 通常のブラウザ仕様ではフォルダの絶対パスは取得できないため、
  // WebView2固有の仕組み(file.path 等)を調査する必要があります。
  // 確認できたら下記のように仮の表示だけ先に動かせます:
  // selectedFolder = "取得したパス";
  // dropZonePath.textContent = selectedFolder;
});

// クリックでもフォルダを選べるようにする(C++側のbrowseFolderを呼び出す)。
// window.browseFolder は、C++でビルドした実行ファイルの中で webview.bind
// により登録された関数なので、通常のブラウザで index.html を直接開いても
// 動作しません(この点、実行ファイル側で確認してください)。
dropZone.addEventListener("click", function () {
  if (typeof window.browseFolder !== "function") {
    console.warn("browseFolder が見つかりません。ビルドしたアプリ側で開いていますか?(通常のブラウザでは動作しません)");
    return;
  }

  window.browseFolder().then(function (path) {
    if (path) {
      selectedFolder = path;
      dropZonePath.textContent = selectedFolder;
    }
  });
});

/* ==========================================================
   ブロックB・C: タグボタン(排他選択 / 独立トグル)
   ========================================================== */

let selectedCategory = null;   // "personal" | "business"
let selectedPlatform = null;   // "desktop" | "web" | "mobile"
let isInternalSystem = false;

function setupExclusiveGroup(groupId, onSelect) {
  const group = document.getElementById(groupId);
  const buttons = group.querySelectorAll(".tag-btn[data-value]");

  buttons.forEach(function (btn) {
    btn.addEventListener("click", function () {
      buttons.forEach(function (b) {
        b.classList.remove("is-selected");
      });
      btn.classList.add("is-selected");
      onSelect(btn.dataset.value);
    });
  });
}

setupExclusiveGroup("category-group", function (value) {
  selectedCategory = value;
});

setupExclusiveGroup("platform-group", function (value) {
  selectedPlatform = value;
});

// 自社システムは他のボタンと独立したON/OFFトグル
const internalToggle = document.getElementById("internal-toggle");
internalToggle.addEventListener("click", function () {
  isInternalSystem = !isInternalSystem;
  internalToggle.classList.toggle("is-selected", isInternalSystem);
});

/* ==========================================================
   ブロックD・E: 詳細記入欄・作成ボタン
   ========================================================== */

function resetCreateForm() {
  selectedFolder = null;
  selectedCategory = null;
  selectedPlatform = null;
  isInternalSystem = false;

  dropZonePath.textContent = "";
  document.getElementById("project-detail").value = "";
  document.querySelectorAll(".tag-btn").forEach(function (btn) {
    btn.classList.remove("is-selected");
  });
}

function onCreateProjectClick() {
  const description = document.getElementById("project-detail").value;

  // ここでの入力チェックは最低限の例です。必要に応じて調整してください。
  if (!selectedFolder) {
    alert("プロジェクトフォルダをドラッグ&ドロップしてください");
    return;
  }
  if (!selectedCategory) {
    alert("カテゴリ(個人用/業務用)を選択してください");
    return;
  }
  if (!selectedPlatform) {
    alert("アプリ形式(デスクトップ/Web/モバイル)を選択してください");
    return;
  }

  const projectData = {
    folderPath: selectedFolder,
    category: selectedCategory,
    platform: selectedPlatform,
    isInternalSystem: isInternalSystem,
    description: description
  };

  if (typeof window.createProject !== "function") {
    console.warn("createProject が見つかりません。ビルドしたアプリ上で確認してください。");
    resetCreateForm();
    goToHomeScreen();
    return;
  }

  window.createProject(projectData).then(function () {
    resetCreateForm();
    goToHomeScreen();
  });
}

/* 初期表示 */
loadProjects();
