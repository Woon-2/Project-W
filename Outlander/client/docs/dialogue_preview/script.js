const DATA_URL = "../../../resources/UI/dialogues/dialogues.json";
const PAGE_SEPARATOR = /\r?\n---\r?\n/g;

const dom = Object.fromEntries([
  "dialogueSelect", "x", "y", "width", "height", "backgroundColor",
  "backgroundAlpha", "textColor", "textAlpha", "padding", "fadeOutSeconds",
  "fontFamily", "fontSize", "pages", "restartButton", "downloadButton",
  "fileInput", "status", "stageFrame", "stage", "dialogueWindow",
  "dialogueText", "pageIndicator"
].map((id) => [id, document.getElementById(id)]));

let documentData;
let current;
let pageIndex = 0;
let fadeTimer = 0;

function rgbaToHex(color) {
  return `#${color.slice(0, 3).map((v) =>
    Math.round(Math.max(0, Math.min(1, v)) * 255).toString(16).padStart(2, "0")
  ).join("")}`;
}

function hexToRgb(hex) {
  return [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16) / 255);
}

function setStatus(message, isError = false) {
  dom.status.textContent = message;
  dom.status.style.color = isError ? "#ff9b9b" : "#80d8a3";
}

async function loadDefault() {
  try {
    const response = await fetch(DATA_URL, { cache: "no-store" });
    if (!response.ok) throw new Error(`${response.status} ${response.statusText}`);
    loadDocument(await response.json());
    setStatus("게임용 dialogues.json을 불러왔습니다.");
  } catch (error) {
    setStatus(`자동 불러오기 실패: ${error.message}. 아래 JSON 불러오기를 사용하세요.`, true);
  }
}

function loadDocument(data) {
  if (!data || !Array.isArray(data.dialogues) || data.dialogues.length === 0) {
    throw new Error("dialogues 배열이 비어 있습니다.");
  }
  documentData = data;
  dom.dialogueSelect.replaceChildren(...data.dialogues.map((dialogue) => {
    const option = document.createElement("option");
    option.value = dialogue.eventId;
    option.textContent = dialogue.eventId;
    return option;
  }));
  selectDialogue(data.dialogues[0].eventId);
}

function selectDialogue(eventId) {
  commitControls();
  current = documentData.dialogues.find((item) => item.eventId === eventId);
  if (!current) return;

  dom.x.value = current.rect.x;
  dom.y.value = current.rect.y;
  dom.width.value = current.rect.width;
  dom.height.value = current.rect.height;
  dom.backgroundColor.value = rgbaToHex(current.background);
  dom.backgroundAlpha.value = current.background[3];
  dom.textColor.value = rgbaToHex(current.textColor);
  dom.textAlpha.value = current.textColor[3];
  dom.padding.value = current.padding;
  dom.fadeOutSeconds.value = current.fadeOutSeconds;
  dom.fontFamily.value = current.fontFamily;
  dom.fontSize.value = current.fontSize;
  dom.pages.value = current.pages.join("\n---\n");
  restart();
}

function commitControls() {
  if (!current) return;
  current.rect = {
    x: Number(dom.x.value),
    y: Number(dom.y.value),
    width: Math.max(1, Number(dom.width.value)),
    height: Math.max(1, Number(dom.height.value))
  };
  current.background = [...hexToRgb(dom.backgroundColor.value), Number(dom.backgroundAlpha.value)];
  current.textColor = [...hexToRgb(dom.textColor.value), Number(dom.textAlpha.value)];
  current.padding = Math.max(0, Number(dom.padding.value));
  current.fadeOutSeconds = Math.max(.01, Number(dom.fadeOutSeconds.value));
  current.fontFamily = dom.fontFamily.value.trim() || "Tahoma";
  current.fontSize = Math.max(1, Number(dom.fontSize.value));
  current.pages = dom.pages.value.split(PAGE_SEPARATOR);
}

function render() {
  if (!current) return;
  commitControls();
  const { rect, background, textColor } = current;
  Object.assign(dom.dialogueWindow.style, {
    left: `${rect.x}px`,
    top: `${rect.y}px`,
    width: `${rect.width}px`,
    height: `${rect.height}px`,
    padding: `${current.padding}px`,
    background: `rgba(${background[0] * 255}, ${background[1] * 255}, ${background[2] * 255}, ${background[3]})`
  });
  Object.assign(dom.dialogueText.style, {
    color: `rgba(${textColor[0] * 255}, ${textColor[1] * 255}, ${textColor[2] * 255}, ${textColor[3]})`,
    fontFamily: `"${current.fontFamily}", sans-serif`,
    fontSize: `${current.fontSize}px`
  });
  pageIndex = Math.min(pageIndex, Math.max(0, current.pages.length - 1));
  dom.dialogueText.textContent = current.pages[pageIndex] || "";
  dom.pageIndicator.textContent = `${pageIndex + 1} / ${current.pages.length}`;
}

function restart() {
  clearTimeout(fadeTimer);
  pageIndex = 0;
  dom.dialogueWindow.classList.remove("is-fading");
  dom.dialogueWindow.style.opacity = "1";
  dom.dialogueWindow.style.transition = "none";
  render();
  dom.stage.focus();
}

function advance() {
  if (!current || dom.dialogueWindow.classList.contains("is-fading")) return;
  if (pageIndex + 1 < current.pages.length) {
    pageIndex += 1;
    render();
    return;
  }
  dom.dialogueWindow.classList.add("is-fading");
  dom.dialogueWindow.style.transition = `opacity ${current.fadeOutSeconds}s linear`;
  requestAnimationFrame(() => { dom.dialogueWindow.style.opacity = "0"; });
  fadeTimer = window.setTimeout(() => {
    setStatus("모든 페이지를 읽어 창이 페이드아웃되었습니다.");
  }, current.fadeOutSeconds * 1000);
}

function fitStage() {
  const bounds = dom.stageFrame.getBoundingClientRect();
  const scale = Math.min((bounds.width - 24) / 1024, (bounds.height - 24) / 768, 1);
  dom.stage.style.transform = `scale(${Math.max(.1, scale)})`;
}

function downloadJson() {
  commitControls();
  const blob = new Blob([`${JSON.stringify(documentData, null, 2)}\n`], {
    type: "application/json"
  });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = "dialogues.json";
  link.click();
  URL.revokeObjectURL(link.href);
  setStatus("dialogues.json을 저장했습니다. resources/UI/dialogues에 덮어쓰면 게임에 반영됩니다.");
}

dom.dialogueSelect.addEventListener("change", () => selectDialogue(dom.dialogueSelect.value));
document.querySelectorAll("input, textarea").forEach((element) => {
  element.addEventListener("input", render);
});
dom.restartButton.addEventListener("click", restart);
dom.downloadButton.addEventListener("click", downloadJson);
dom.stage.addEventListener("mousedown", (event) => {
  if (event.button === 0) advance();
});
document.addEventListener("keydown", (event) => {
  if (event.key !== "Enter" || event.target.matches("textarea, input, select, button")) return;
  event.preventDefault();
  advance();
});
dom.fileInput.addEventListener("change", async () => {
  const [file] = dom.fileInput.files;
  if (!file) return;
  try {
    loadDocument(JSON.parse(await file.text()));
    setStatus(`${file.name}을 불러왔습니다.`);
  } catch (error) {
    setStatus(`JSON 불러오기 실패: ${error.message}`, true);
  }
});

new ResizeObserver(fitStage).observe(dom.stageFrame);
loadDefault();
