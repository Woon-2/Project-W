const MAX_PLAYERS = 4;
const PLAYER_ID_LOCAL = "local-player";
const alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

const mockRooms = {
  JOIN01: {
    code: "JOIN01",
    hostId: "host-join01",
    playerList: [
      { id: "host-join01", name: "Host_01" },
      { id: "local-player", name: "나" }
    ]
  },
  TEST12: {
    code: "TEST12",
    hostId: "host-test12",
    playerList: [
      { id: "host-test12", name: "Host_Test" },
      { id: "dummy-alpha", name: "Alpha" },
      { id: "local-player", name: "나" }
    ]
  }
};

const state = {
  roomCode: "",
  isHost: false,
  hostId: "",
  players: [],
  dummySeed: 1,
  toastTimer: 0
};

const dom = {
  mainView: document.getElementById("mainView"),
  lobbyView: document.getElementById("lobbyView"),
  createRoomButton: document.getElementById("createRoomButton"),
  joinRoomForm: document.getElementById("joinRoomForm"),
  roomCodeInput: document.getElementById("roomCodeInput"),
  settingsButton: document.getElementById("settingsButton"),
  quitButton: document.getElementById("quitButton"),
  roomCodeText: document.getElementById("roomCodeText"),
  copyCodeButton: document.getElementById("copyCodeButton"),
  playerSlots: document.getElementById("playerSlots"),
  playerCount: document.getElementById("playerCount"),
  hostActionArea: document.getElementById("hostActionArea"),
  leaveRoomButton: document.getElementById("leaveRoomButton"),
  addDummyButton: document.getElementById("addDummyButton"),
  removeDummyButton: document.getElementById("removeDummyButton"),
  toast: document.getElementById("toast"),
  transitionOverlay: document.getElementById("transitionOverlay")
};

function C_CreateRoom() {
  const code = generateRoomCode();
  S_CreateRoom(code);
}

function S_CreateRoom(code) {
  state.roomCode = code;
  state.isHost = true;
  state.hostId = PLAYER_ID_LOCAL;
  state.players = [{ id: PLAYER_ID_LOCAL, name: "나" }];
  state.dummySeed = 1;

  openLobbyView();
  showToast("방이 생성되었습니다");
}

function C_JoinRoom(code) {
  const normalizedCode = sanitizeRoomCode(code);

  if (normalizedCode.length !== 6) {
    S_JoinRoom(false, 0, "", normalizedCode, [], "6자리 코드를 입력하세요");
    return;
  }

  if (normalizedCode === "FULL00") {
    S_JoinRoom(false, MAX_PLAYERS, "", normalizedCode, [], "방이 가득 찼습니다");
    return;
  }

  const mockRoom = mockRooms[normalizedCode];
  if (!mockRoom) {
    S_JoinRoom(false, 0, "", normalizedCode, [], "방을 찾을 수 없습니다");
    return;
  }

  const playerList = clonePlayers(mockRoom.playerList);
  const hasLocalPlayer = playerList.some((player) => player.id === PLAYER_ID_LOCAL);

  if (!hasLocalPlayer) {
    playerList.push({ id: PLAYER_ID_LOCAL, name: "나" });
  }

  S_JoinRoom(true, playerList.length, mockRoom.hostId, mockRoom.code, playerList);
}

function S_JoinRoom(success, playerCnt, hostId, code, playerList, errorMessage) {
  if (!success) {
    showToast(errorMessage || "방 참가에 실패했습니다", "error");
    return;
  }

  state.roomCode = code;
  state.isHost = hostId === PLAYER_ID_LOCAL;
  state.hostId = hostId;
  state.players = playerList.slice(0, MAX_PLAYERS);
  state.dummySeed = playerCnt;

  openLobbyView();
  showToast("방에 참가했습니다");
}

function S_LobbyRoomPlayerJoined(player) {
  if (state.players.length >= MAX_PLAYERS) {
    showToast("방이 가득 찼습니다", "error");
    return;
  }

  state.players.push(player);
  renderLobby();
  showToast(`${player.name} 입장`);
}

function S_LobbyRoomPlayerLeft(playerId) {
  const leavingPlayer = state.players.find((player) => player.id === playerId);
  state.players = state.players.filter((player) => player.id !== playerId);

  if (leavingPlayer && leavingPlayer.id === state.hostId && state.players.length > 0) {
    state.hostId = state.players[0].id;
    state.isHost = state.hostId === PLAYER_ID_LOCAL;
  }

  renderLobby();

  if (leavingPlayer) {
    showToast(`${leavingPlayer.name} 퇴장`);
  }
}

function C_LeaveRoom() {
  resetRoomState();
  openMainView();
  showToast("방에서 나갔습니다");
}

function C_GameStart() {
  if (!state.isHost) {
    showToast("호스트만 시작할 수 있습니다", "error");
    return;
  }

  S_GameStart("127.0.0.1", 7777, state.roomCode);
}

function S_GameStart(roomServerIp, roomServerPort, lobbyCode) {
  dom.transitionOverlay.classList.remove("is-hidden");

  window.setTimeout(() => {
    dom.transitionOverlay.classList.add("is-hidden");
    showToast(`${roomServerIp}:${roomServerPort} / ${lobbyCode}`);
  }, 1300);
}

function generateRoomCode() {
  let code = "";

  for (let i = 0; i < 6; i += 1) {
    code += alphabet[Math.floor(Math.random() * alphabet.length)];
  }

  return code;
}

function sanitizeRoomCode(code) {
  return code.trim().toUpperCase().replace(/[^A-Z0-9]/g, "");
}

function openMainView() {
  dom.mainView.classList.remove("is-hidden");
  dom.lobbyView.classList.add("is-hidden");
  dom.roomCodeInput.value = "";
  dom.roomCodeInput.focus();
}

function openLobbyView() {
  dom.mainView.classList.add("is-hidden");
  dom.lobbyView.classList.remove("is-hidden");
  renderLobby();
}

function renderLobby() {
  dom.roomCodeText.textContent = state.roomCode || "------";
  dom.playerCount.textContent = `${state.players.length} / ${MAX_PLAYERS}`;
  dom.playerSlots.innerHTML = "";

  for (let index = 0; index < MAX_PLAYERS; index += 1) {
    const player = state.players[index];
    dom.playerSlots.appendChild(createPlayerSlot(player, index));
  }

  renderHostAction();
  dom.addDummyButton.disabled = state.players.length >= MAX_PLAYERS;
  dom.removeDummyButton.disabled = getRemovableDummyPlayers().length === 0;
}

function createPlayerSlot(player, index) {
  const slot = document.createElement("div");
  slot.className = "player-slot";

  const slotNumber = document.createElement("div");
  slotNumber.className = "slot-number";
  slotNumber.textContent = String(index + 1).padStart(2, "0");

  const modelBay = document.createElement("div");
  modelBay.className = "model-bay";

  const character = document.createElement("div");
  character.className = "slot-character";

  [
    ["head", "model-head"],
    ["torso", "model-torso"],
    ["arm", "model-arm model-arm-left"],
    ["arm", "model-arm model-arm-right"],
    ["leg", "model-leg model-leg-left"],
    ["leg", "model-leg model-leg-right"]
  ].forEach(([, className]) => {
    const part = document.createElement("div");
    part.className = `model-part ${className}`;
    character.appendChild(part);
  });

  const platform = document.createElement("div");
  platform.className = "model-platform";

  modelBay.append(character, platform);

  const info = document.createElement("div");
  info.className = "player-nameplate";

  const name = document.createElement("strong");
  name.className = "player-name";

  const badge = document.createElement("div");
  badge.className = "host-badge";

  if (!player) {
    slot.classList.add("is-empty");
    name.textContent = "대기 중";
    badge.textContent = "";
  } else {
    name.textContent = player.name;
    badge.textContent = player.id === state.hostId ? "👑 호스트" : "";
  }

  info.append(name, badge);
  slot.append(slotNumber, modelBay, info);
  return slot;
}

function renderHostAction() {
  dom.hostActionArea.innerHTML = "";

  if (state.isHost) {
    const startButton = document.createElement("button");
    startButton.className = "button button-primary start-button";
    startButton.type = "button";
    startButton.textContent = "게임 시작";
    startButton.addEventListener("click", C_GameStart);
    dom.hostActionArea.appendChild(startButton);
    return;
  }

  const waitMessage = document.createElement("div");
  waitMessage.className = "wait-message";
  waitMessage.textContent = "호스트가 시작하기를 기다리는 중...";
  dom.hostActionArea.appendChild(waitMessage);
}

function addDummyPlayer() {
  const nextNumber = state.dummySeed + 1;
  state.dummySeed = nextNumber;

  S_LobbyRoomPlayerJoined({
    id: `dummy-${Date.now()}-${nextNumber}`,
    name: `Player_${String(nextNumber).padStart(2, "0")}`
  });
}

function removeDummyPlayer() {
  const removablePlayers = getRemovableDummyPlayers();
  const player = removablePlayers[removablePlayers.length - 1];

  if (!player) {
    showToast("제거할 더미가 없습니다", "error");
    return;
  }

  S_LobbyRoomPlayerLeft(player.id);
}

function getRemovableDummyPlayers() {
  return state.players.filter((player) => player.id !== PLAYER_ID_LOCAL && player.id !== state.hostId);
}

function resetRoomState() {
  state.roomCode = "";
  state.isHost = false;
  state.hostId = "";
  state.players = [];
  state.dummySeed = 1;
}

function clonePlayers(players) {
  return players.map((player) => ({ ...player }));
}

function showToast(message, type = "normal") {
  window.clearTimeout(state.toastTimer);
  dom.toast.textContent = message;
  dom.toast.classList.toggle("is-error", type === "error");
  dom.toast.classList.add("is-visible");

  state.toastTimer = window.setTimeout(() => {
    dom.toast.classList.remove("is-visible");
  }, 2200);
}

async function copyRoomCode() {
  if (!state.roomCode) {
    return;
  }

  try {
    if (navigator.clipboard && window.isSecureContext) {
      await navigator.clipboard.writeText(state.roomCode);
    } else {
      copyTextFallback(state.roomCode);
    }

    showToast("방 코드가 복사되었습니다");
  } catch (error) {
    showToast("복사에 실패했습니다", "error");
  }
}

function copyTextFallback(text) {
  const textarea = document.createElement("textarea");
  textarea.value = text;
  textarea.setAttribute("readonly", "");
  textarea.style.position = "fixed";
  textarea.style.opacity = "0";
  document.body.appendChild(textarea);
  textarea.select();
  document.execCommand("copy");
  textarea.remove();
}

dom.createRoomButton.addEventListener("click", C_CreateRoom);
dom.joinRoomForm.addEventListener("submit", (event) => {
  event.preventDefault();
  C_JoinRoom(dom.roomCodeInput.value);
});
dom.roomCodeInput.addEventListener("input", () => {
  dom.roomCodeInput.value = sanitizeRoomCode(dom.roomCodeInput.value);
});
dom.settingsButton.addEventListener("click", () => showToast("설정 화면 준비 중"));
dom.quitButton.addEventListener("click", () => showToast("타이틀로 돌아갑니다"));
dom.copyCodeButton.addEventListener("click", copyRoomCode);
dom.leaveRoomButton.addEventListener("click", C_LeaveRoom);
dom.addDummyButton.addEventListener("click", addDummyPlayer);
dom.removeDummyButton.addEventListener("click", removeDummyPlayer);
