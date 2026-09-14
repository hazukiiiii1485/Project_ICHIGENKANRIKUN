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

// 本来はここで C++ 側の get_all_projects() をブリッジ経由で呼び出します。
// webviewライブラリの webview_bind で登録した関数名に合わせて置き換えてください。
// 例: const projects = await window.getProjects();
function fetchProjectsFromBackend() {
  // ---- ここから仮データ(バックエンド実装までのダミー) ----
  return Promise.resolve([
    {
      id: "proj_001",
      name: "在庫管理システム",
      description: "倉庫の入出庫を社内スタッフ向けに一元化",
      category: "business",       // "personal" | "business"
      platform: "desktop",        // "desktop" | "web" | "mobile"
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
  // ---- ここまで仮データ ----
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

// 詳細画面(今後実装)への遷移。現時点ではIDを保持するだけ。
let currentProjectId = null;
function goToDetailScreen(projectId) {
  currentProjectId = projectId;
  // TODO: 詳細画面実装時に showView("view-detail") とデータ読み込みを行う
  console.log("詳細画面へ遷移予定:", projectId);
}

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

  // TODO: ここでC++ブリッジ(webview_bindで登録した関数)へ送信する。
  // 例: window.createProject(projectData).then(() => { ... });
  console.log("作成するプロジェクト:", projectData);

  resetCreateForm();
  goToHomeScreen();
}

/* 初期表示 */
loadProjects();
