import "./style.css";

import { TowerSim } from "./sim/index";
import type { SimCommand } from "./sim/commands";
import type { SimSnapshot } from "./sim/snapshot";
import {
  exportLegacyTdt,
  LegacyExportError,
} from "./sim/legacy-tdt/export";
import {
  importLegacyTdt,
  LegacyImportError,
  looksLikeLegacyTdt,
  type LegacyTdtImportReport,
} from "./sim/legacy-tdt/import";
import { TILE_COSTS, TILE_STAR_REQUIREMENTS, TILE_WIDTHS } from "./sim/resources";
import { GROUND_Y, GRID_HEIGHT, GRID_WIDTH, yToFloor } from "./sim/world";

const TILE_PX = 16;
const FLOOR_PX = 24;
const SAVE_KEY = "simtower-native-save-v1";

type ToolDefinition = {
  id: string;
  label: string;
  resource?: number;
};

const TOOLS: ToolDefinition[] = [
  { id: "inspect", label: "Inspect" },
  { id: "bulldoze", label: "Bulldoze" },
  { id: "floor", label: "Floor", resource: 5000 },
  { id: "lobby", label: "Lobby" },
  { id: "stairs", label: "Stairs", resource: 2408 },
  { id: "escalator", label: "Escalator", resource: 2728 },
  { id: "elevator", label: "Elevator", resource: 1064 },
  { id: "elevatorExpress", label: "Express", resource: 1065 },
  { id: "elevatorService", label: "Service", resource: 1066 },
  { id: "office", label: "Office", resource: 1448 },
  { id: "condo", label: "Condo", resource: 1576 },
  { id: "hotelSingle", label: "Single", resource: 1192 },
  { id: "hotelTwin", label: "Twin", resource: 1256 },
  { id: "hotelSuite", label: "Suite", resource: 1320 },
  { id: "housekeeping", label: "Housekeeping", resource: 1960 },
  { id: "fastFood", label: "Fast Food", resource: 1768 },
  { id: "restaurant", label: "Restaurant", resource: 1384 },
  { id: "retail", label: "Retail", resource: 1640 },
  { id: "partyHall", label: "Party Hall", resource: 2856 },
  { id: "cinema", label: "Cinema", resource: 2152 },
  { id: "parking", label: "Parking", resource: 1704 },
  { id: "parkingRamp", label: "Ramp", resource: 3816 },
  { id: "recyclingCenter", label: "Recycling", resource: 2280 },
  { id: "security", label: "Security", resource: 1896 },
  { id: "medical", label: "Medical", resource: 1832 },
  { id: "metro", label: "Metro", resource: 2984 },
  { id: "cathedral", label: "Cathedral", resource: 3304 },
];

const app = document.querySelector<HTMLDivElement>("#app");
if (!app) throw new Error("missing app root");

app.innerHTML = `
  <main class="app">
    <header class="titlebar">
      <div class="brand"><img src="./bitmaps/257.png" alt="SimTower"><span>Native Port</span></div>
      <div class="hud">
        <span>Cash <strong id="cash" class="hud-value"></strong></span>
        <span>Population <strong id="population" class="hud-value"></strong></span>
        <span id="stars" class="stars"></span>
        <span>Tick <strong id="tick" class="hud-value"></strong></span>
        <button id="pause">Pause</button>
        <select id="speed" aria-label="simulation speed">
          <option value="1">1×</option><option value="4">4×</option><option value="16">16×</option>
        </select>
        <button id="sound" aria-pressed="true">Sound On</button><button id="save">Save</button><button id="load">Load</button><button id="export">Export…</button><button id="export-tdt">Export .TDT</button><button id="import">Import…</button><button id="new">New</button><button id="help-button">Help</button>
        <input id="import-file" type="file" accept=".tdt,.TDT,.json,.stn,application/json,application/octet-stream" hidden>
      </div>
    </header>
    <nav id="toolbar" class="toolbar" aria-label="construction tools"></nav>
    <section class="viewport">
      <canvas id="tower"></canvas>
      <div id="status" class="status">Loading original assets…</div>
      <div class="help">Wheel: zoom · Middle/right drag: pan · Left click/drag: build</div>
      <aside id="inspector" class="inspector" hidden>
        <header><h2 id="inspector-title">Inspect</h2><button id="inspector-close" type="button">Close</button></header>
        <div id="inspector-content" class="inspector-content"></div>
      </aside>
    </section>
    <div id="confirmation" class="modal-backdrop" hidden>
      <section class="modal" role="alertdialog" aria-modal="true" aria-labelledby="confirmation-title">
        <h2 id="confirmation-title">SimTower</h2>
        <img id="confirmation-image" class="confirmation-image" alt="" hidden>
        <p id="confirmation-message"></p>
        <div class="modal-actions">
          <button id="confirmation-no" type="button">Cancel</button>
          <button id="confirmation-yes" type="button">OK</button>
        </div>
      </section>
    </div>
    <div id="help-browser" class="help-backdrop" hidden>
      <section class="help-window" role="dialog" aria-modal="true" aria-labelledby="help-title">
        <header class="help-titlebar">
          <h2 id="help-title">SimTower Help</h2>
          <button id="help-close" type="button" aria-label="Close help">Close</button>
        </header>
        <div class="help-layout">
          <aside class="help-index">
            <input id="help-search" type="search" placeholder="Search 99 topics…" aria-label="Search help">
            <div id="help-topics" class="help-topics" role="listbox" aria-label="Help topics"></div>
          </aside>
          <article class="help-article">
            <h3 id="help-topic-title">Loading manual…</h3>
            <div id="help-images" class="help-images"></div>
            <div id="help-body"></div>
          </article>
        </div>
      </section>
    </div>
  </main>`;

const canvas = document.querySelector<HTMLCanvasElement>("#tower")!;
const ctx = canvas.getContext("2d", { alpha: false })!;
const toolbar = document.querySelector<HTMLElement>("#toolbar")!;
const statusElement = document.querySelector<HTMLElement>("#status")!;
const cashElement = document.querySelector<HTMLElement>("#cash")!;
const populationElement = document.querySelector<HTMLElement>("#population")!;
const starsElement = document.querySelector<HTMLElement>("#stars")!;
const tickElement = document.querySelector<HTMLElement>("#tick")!;
const pauseButton = document.querySelector<HTMLButtonElement>("#pause")!;
const speedSelect = document.querySelector<HTMLSelectElement>("#speed")!;
const soundButton = document.querySelector<HTMLButtonElement>("#sound")!;
const confirmationElement = document.querySelector<HTMLElement>("#confirmation")!;
const confirmationMessage = document.querySelector<HTMLElement>("#confirmation-message")!;
const confirmationImage = document.querySelector<HTMLImageElement>("#confirmation-image")!;
const confirmationYes = document.querySelector<HTMLButtonElement>("#confirmation-yes")!;
const confirmationNo = document.querySelector<HTMLButtonElement>("#confirmation-no")!;
const helpBrowser = document.querySelector<HTMLElement>("#help-browser")!;
const helpSearch = document.querySelector<HTMLInputElement>("#help-search")!;
const helpTopicsElement = document.querySelector<HTMLElement>("#help-topics")!;
const helpTopicTitle = document.querySelector<HTMLElement>("#help-topic-title")!;
const helpImages = document.querySelector<HTMLElement>("#help-images")!;
const helpBody = document.querySelector<HTMLElement>("#help-body")!;
const inspector = document.querySelector<HTMLElement>("#inspector")!;
const inspectorTitle = document.querySelector<HTMLElement>("#inspector-title")!;
const inspectorContent = document.querySelector<HTMLElement>("#inspector-content")!;

type HelpTopic = {
  id: number;
  title: string;
  context: string;
  heading: string;
  paragraphs: string[];
  images: string[];
};

let sim = TowerSim.create("local", "Tower", "perfect-parity");
let selectedTool = "inspect";
let paused = false;
let speed = 1;
let zoom = 1;
let cameraX = 0;
let cameraY = GROUND_Y * FLOOR_PX - 520;
let dirty = true;
let lastHudUpdate = 0;
let dragging = false;
let dragButton = -1;
let dragStartX = 0;
let dragStartY = 0;
let dragCameraX = 0;
let dragCameraY = 0;
let rangeBuilding = false;
let rangeStart = { x: 0, y: 0 };
let confirmationOpen = false;
let helpOpen = false;
let soundEnabled = true;
let inspectorOpen = false;
let inspectorCell: { x: number; y: number } | null = null;
let helpTopics: HelpTopic[] = [];
let existingResourceIds = new Set<number>();

const imageCache = new Map<number, HTMLImageElement>();

function assetImage(resourceId: number): HTMLImageElement {
  let image = imageCache.get(resourceId);
  if (!image) {
    image = new Image();
    image.decoding = "async";
    image.src = `./bitmaps/${resourceId}.png`;
    image.addEventListener("load", () => { dirty = true; });
    imageCache.set(resourceId, image);
  }
  return image;
}

function formatMoney(value: number): string {
  return new Intl.NumberFormat("en-US", {
    style: "currency", currency: "USD", maximumFractionDigits: 0,
  }).format(value);
}

function playWave(resourceId: number): void {
  if (!soundEnabled) return;
  const audio = new Audio(`./audio/${resourceId}.wav`);
  audio.volume = .65;
  void audio.play().catch(() => {
    // WebView2 may defer playback until the first user gesture; later events retry normally.
  });
}

function playNotificationSound(
  kind: string,
  message: string,
  soundResourceId?: number,
): void {
  if (soundResourceId !== undefined) {
    playWave(soundResourceId);
  } else if (kind === "star_advanced") {
    playWave(10008);
  } else if (message === "metro_train_arrival") {
    playWave(10010);
  } else if (message.startsWith("Ransom paid")) {
    playWave(10015);
  } else if (message.startsWith("Helicopter dispatched")) {
    playWave(10006);
  }
}

function updateHud(force = false): void {
  const now = performance.now();
  if (!force && now - lastHudUpdate < 100) return;
  lastHudUpdate = now;
  cashElement.textContent = formatMoney(sim.cash);
  populationElement.textContent = sim.currentPopulation.toLocaleString();
  tickElement.textContent = sim.simTime.toLocaleString();
  starsElement.textContent = "★".repeat(Math.max(0, Math.min(5, sim.starCount))) + "☆".repeat(Math.max(0, 5 - sim.starCount));
  for (const button of toolbar.querySelectorAll<HTMLButtonElement>("button[data-tool]")) {
    const requirement = TILE_STAR_REQUIREMENTS[button.dataset.tool ?? ""] ?? 1;
    button.disabled = !sim.freeBuild && requirement > sim.starCount;
  }
}

function setStatus(message: string): void {
  statusElement.textContent = message;
}

function askConfirmation(message: string, imageResourceId?: number): Promise<boolean> {
  confirmationMessage.textContent = message;
  confirmationImage.hidden = imageResourceId === undefined;
  confirmationImage.src = imageResourceId === undefined
    ? ""
    : `./bitmaps/${imageResourceId}.png`;
  confirmationElement.hidden = false;
  confirmationOpen = true;
  confirmationYes.focus();
  return new Promise((resolve) => {
    const finish = (accepted: boolean) => {
      confirmationElement.hidden = true;
      confirmationOpen = false;
      confirmationYes.removeEventListener("click", accept);
      confirmationNo.removeEventListener("click", reject);
      resolve(accepted);
    };
    const accept = () => finish(true);
    const reject = () => finish(false);
    confirmationYes.addEventListener("click", accept);
    confirmationNo.addEventListener("click", reject);
  });
}

function showHelpTopic(topic: HelpTopic): void {
  helpTopicTitle.textContent = topic.title;
  helpImages.replaceChildren();
  for (const source of topic.images) {
    const image = document.createElement("img");
    image.src = `./help/${source}`;
    image.alt = `${topic.title} illustration`;
    helpImages.append(image);
  }
  helpBody.replaceChildren();
  const paragraphs = topic.heading && topic.heading !== topic.title
    ? [topic.heading, ...topic.paragraphs]
    : topic.paragraphs;
  for (const [index, text] of paragraphs.entries()) {
    const paragraph = document.createElement("p");
    paragraph.textContent = text;
    if (index === 0 && topic.heading !== topic.title) paragraph.className = "help-lead";
    helpBody.append(paragraph);
  }
  for (const button of helpTopicsElement.querySelectorAll<HTMLButtonElement>("button")) {
    button.classList.toggle("selected", Number(button.dataset.topicId) === topic.id);
  }
}

function renderHelpIndex(query = ""): void {
  const terms = query.trim().toLocaleLowerCase();
  const matches = helpTopics
    .map((topic) => {
      const title = topic.title.toLocaleLowerCase();
      const body = `${topic.heading} ${topic.paragraphs.join(" ")}`.toLocaleLowerCase();
      const score = !terms || title === terms ? 0 : title.startsWith(terms) ? 1 : title.includes(terms) ? 2 : body.includes(terms) ? 3 : 4;
      return { topic, score };
    })
    .filter(({ score }) => score < 4)
    .sort((left, right) => left.score - right.score || left.topic.title.localeCompare(right.topic.title))
    .map(({ topic }) => topic);
  helpTopicsElement.replaceChildren();
  for (const topic of matches) {
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.topicId = String(topic.id);
    button.textContent = topic.title;
    button.addEventListener("click", () => showHelpTopic(topic));
    helpTopicsElement.append(button);
  }
  if (matches.length) showHelpTopic(matches[0]);
}

async function loadHelp(): Promise<void> {
  const response = await fetch("./help/topics.json");
  const payload = await response.json() as { topics: HelpTopic[] };
  helpTopics = payload.topics;
  renderHelpIndex();
}

function openHelp(): void {
  helpBrowser.hidden = false;
  helpOpen = true;
  helpSearch.focus();
}

function closeHelp(): void {
  helpBrowser.hidden = true;
  helpOpen = false;
  pauseButton.focus();
}

function floorLabel(floor: number): string {
  const visible = floor - 10;
  return visible === 0 ? "L" : String(visible);
}

function appendInspectorStat(label: string, value: string | number): void {
  const row = document.createElement("div");
  row.className = "inspector-stat";
  const term = document.createElement("span");
  const data = document.createElement("strong");
  term.textContent = label;
  data.textContent = String(value);
  row.append(term, data);
  inspectorContent.append(row);
}

async function submitInspectorCommand(command: SimCommand, success: string): Promise<void> {
  const result = sim.submitCommand(command);
  if (!result.accepted) {
    setStatus(`Cannot change facility: ${result.reason ?? "rejected"}`);
    return;
  }
  for (const prompt of sim.drainPrompts()) {
    const accepted = await askConfirmation(prompt.message + (prompt.cost ? ` Cost: ${formatMoney(prompt.cost)}.` : ""));
    sim.submitCommand({ type: "prompt_response", promptId: prompt.promptId, accepted });
  }
  dirty = true;
  updateHud(true);
  setStatus(success);
  renderInspector();
}

function renderInspector(): void {
  if (!inspectorCell) return;
  const { x, y } = inspectorCell;
  const info = sim.queryCell(x, y);
  const floor = yToFloor(y);
  const definition = TOOLS.find((tool) => tool.id === info.tileType);
  inspectorTitle.textContent = `${definition?.label ?? info.tileType} · floor ${floorLabel(floor)}`;
  inspectorContent.replaceChildren();
  appendInspectorStat("Column", info.anchorX);
  appendInspectorStat("Type", info.tileType);

  if (info.objectInfo) {
    appendInspectorStat("State", info.objectInfo.unitStatus);
    appendInspectorStat("Evaluation", info.objectInfo.evalLevel);
    appendInspectorStat("Active ticks", info.objectInfo.activationTickCount.toLocaleString());
    if (info.objectInfo.venueAvailability !== undefined) {
      appendInspectorStat("Availability", info.objectInfo.venueAvailability);
    }
    const rentFamilies = new Set([3, 4, 5, 6, 7, 9, 10, 12]);
    if (rentFamilies.has(info.objectInfo.objectTypeCode)) {
      const section = document.createElement("section");
      section.innerHTML = "<h3>Rent / price</h3>";
      const select = document.createElement("select");
      for (let level = 0; level <= 3; level++) {
        const option = document.createElement("option");
        option.value = String(level);
        option.textContent = `Level ${level}`;
        option.selected = info.objectInfo.rentLevel === level;
        select.append(option);
      }
      const apply = document.createElement("button");
      apply.type = "button";
      apply.textContent = "Apply";
      apply.addEventListener("click", () => void submitInspectorCommand(
        { type: "set_rent_level", x: info.anchorX, y, rentLevel: Number(select.value) },
        "Facility price updated.",
      ));
      section.append(select, apply);
      inspectorContent.append(section);
    }
  }

  if (info.cinemaInfo) {
    const section = document.createElement("section");
    section.innerHTML = `<h3>Cinema program</h3><p>Movie selector ${info.cinemaInfo.selector} · attendance ${info.cinemaInfo.attendanceCounter}</p>`;
    for (const pool of ["classic", "new"] as const) {
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = pool === "classic" ? "Next classic ($1M)" : "Next new ($5M)";
      button.addEventListener("click", () => void submitInspectorCommand(
        { type: "set_cinema_movie_pool", x: info.anchorX, y, pool },
        `${pool === "classic" ? "Classic" : "New"} movie selected.`,
      ));
      section.append(button);
    }
    inspectorContent.append(section);
  }

  if (info.carrierInfo) {
    const carrier = info.carrierInfo;
    const section = document.createElement("section");
    section.innerHTML = `<h3>Elevator shaft</h3><p>${carrier.carCount}/${carrier.maxCars} cars · floors ${floorLabel(carrier.bottomServedFloor)}–${floorLabel(carrier.topServedFloor)}</p>`;
    const add = document.createElement("button");
    add.type = "button";
    add.textContent = "Add car";
    add.addEventListener("click", () => void submitInspectorCommand(
      { type: "add_elevator_car", x: carrier.column, y }, "Elevator car added.",
    ));
    const remove = document.createElement("button");
    remove.type = "button";
    remove.textContent = "Remove car";
    remove.addEventListener("click", () => void submitInspectorCommand(
      { type: "remove_elevator_car", x: carrier.column, y }, "Elevator car removed.",
    ));
    section.append(add, remove);

    const settings = document.createElement("div");
    settings.className = "inspector-settings";
    const dwell = document.createElement("input");
    dwell.type = "number";
    dwell.min = "0";
    dwell.max = "255";
    dwell.value = String(carrier.dwellDelay);
    const dwellButton = document.createElement("button");
    dwellButton.type = "button";
    dwellButton.textContent = "Set dwell";
    dwellButton.addEventListener("click", () => void submitInspectorCommand(
      { type: "set_elevator_dwell_delay", x: carrier.column, y, value: Number(dwell.value) },
      "Elevator dwell delay updated.",
    ));
    const response = document.createElement("input");
    response.type = "number";
    response.min = "0";
    response.max = "255";
    response.value = String(carrier.waitingCarResponseThreshold);
    const responseButton = document.createElement("button");
    responseButton.type = "button";
    responseButton.textContent = "Set response";
    responseButton.addEventListener("click", () => void submitInspectorCommand(
      { type: "set_elevator_waiting_car_response", x: carrier.column, y, value: Number(response.value) },
      "Waiting-car response updated.",
    ));
    settings.append(dwell, dwellButton, response, responseButton);
    section.append(settings);

    const cars = document.createElement("div");
    cars.className = "inspector-cars";
    carrier.carInfos.forEach((car, index) => {
      if (!car.active) return;
      const label = document.createElement("label");
      label.textContent = `Car ${index + 1} home `;
      const select = document.createElement("select");
      for (const servedFloor of carrier.servedFloors) {
        const option = document.createElement("option");
        option.value = String(servedFloor);
        option.textContent = floorLabel(servedFloor);
        option.selected = car.homeFloor === servedFloor;
        select.append(option);
      }
      select.addEventListener("change", () => void submitInspectorCommand(
        { type: "set_elevator_home_floor", x: carrier.column, carIndex: index, floor: Number(select.value) },
        `Car ${index + 1} home floor updated.`,
      ));
      label.append(select);
      cars.append(label);
    });
    section.append(cars);

    const stops = document.createElement("div");
    stops.className = "inspector-stops";
    for (const [index, servedFloor] of carrier.servedFloors.entries()) {
      const button = document.createElement("button");
      button.type = "button";
      button.className = carrier.stopFloorEnabled[index] ? "enabled" : "disabled";
      button.textContent = floorLabel(servedFloor);
      button.title = carrier.stopFloorEnabled[index] ? "Stop enabled" : "Stop skipped";
      button.addEventListener("click", () => void submitInspectorCommand(
        { type: "toggle_elevator_floor_stop", x: carrier.column, floor: servedFloor },
        `Elevator stop ${floorLabel(servedFloor)} toggled.`,
      ));
      stops.append(button);
    }
    section.append(stops);
    inspectorContent.append(section);
  }

  if (!info.objectInfo && !info.carrierInfo) {
    const empty = document.createElement("p");
    empty.textContent = "No facility or elevator occupies this cell.";
    inspectorContent.append(empty);
  }
}

function openInspector(x: number, y: number): void {
  inspectorCell = { x, y };
  inspector.hidden = false;
  inspectorOpen = true;
  renderInspector();
}

function closeInspector(): void {
  inspector.hidden = true;
  inspectorOpen = false;
  inspectorCell = null;
}

function setSelectedTool(tool: string): void {
  selectedTool = tool;
  canvas.style.cursor = tool === "inspect"
    ? 'url("./win16-ui/cursors/1003.cur"), crosshair'
    : 'url("./win16-ui/cursors/1004.cur"), pointer';
  for (const button of toolbar.querySelectorAll<HTMLButtonElement>("button[data-tool]")) {
    button.classList.toggle("selected", button.dataset.tool === selectedTool);
  }
  const definition = TOOLS.find((item) => item.id === tool);
  if (definition && TILE_COSTS[tool] !== undefined) {
    setStatus(`${definition.label}: ${TILE_WIDTHS[tool] ?? 1} tiles · ${formatMoney(TILE_COSTS[tool])}`);
  } else {
    setStatus(definition?.label ?? tool);
  }
}

function buildToolbar(): void {
  const groups = [2, 8, 14, 19, 23];
  TOOLS.forEach((tool, index) => {
    if (groups.includes(index)) {
      const separator = document.createElement("span");
      separator.className = "separator";
      toolbar.append(separator);
    }
    const button = document.createElement("button");
    button.type = "button";
    button.dataset.tool = tool.id;
    button.textContent = tool.label;
    button.title = tool.resource ? `${tool.label} · original bitmap ${tool.resource}` : tool.label;
    button.addEventListener("click", () => setSelectedTool(tool.id));
    toolbar.append(button);
  });
  setSelectedTool(selectedTool);
}

function resizeCanvas(): void {
  const ratio = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const width = Math.max(1, Math.round(rect.width * ratio));
  const height = Math.max(1, Math.round(rect.height * ratio));
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
    dirty = true;
  }
}

function resourceForCell(tileType: string, x: number, y: number): number | null {
  const overrides: Record<string, number> = {
    floor: 5000,
    stairs: 2408,
    escalator: 2728,
    parking: 1704,
    parkingRamp: 3816,
    elevatorExpress: 1065,
    elevatorService: 1066,
  };
  if (overrides[tileType] && existingResourceIds.has(overrides[tileType])) return overrides[tileType];
  const info = sim.queryCell(x, y);
  const family = info.objectInfo?.objectTypeCode;
  if (family !== undefined) {
    const candidate = family * 64 + 1000;
    if (existingResourceIds.has(candidate)) return candidate;
  }
  const tool = TOOLS.find((item) => item.id === tileType);
  return tool?.resource && existingResourceIds.has(tool.resource) ? tool.resource : null;
}

function resourceForFacility(tileType: string, family: number): number | null {
  const overrides: Record<string, number> = {
    parking: 1704,
    parkingRamp: 3816,
  };
  const override = overrides[tileType];
  if (override && existingResourceIds.has(override)) return override;
  const candidate = family * 64 + 1000;
  if (existingResourceIds.has(candidate)) return candidate;
  const tool = TOOLS.find((item) => item.id === tileType);
  return tool?.resource && existingResourceIds.has(tool.resource) ? tool.resource : null;
}

function drawFacility(
  tileType: string,
  x: number,
  y: number,
  width = TILE_WIDTHS[tileType] ?? 1,
  resource = resourceForCell(tileType, x, y),
  visualState = 0,
): void {
  const dx = x * TILE_PX;
  const dy = y * FLOOR_PX;
  const dw = width * TILE_PX;
  if (resource !== null) {
    const image = assetImage(resource);
    if (image.complete && image.naturalWidth) {
      // Original world DIBs use 8 horizontal pixels per placement cell and
      // frequently store several visual states side-by-side.
      const sourceWidth = Math.min(width * 8, image.naturalWidth);
      const frameCount = Math.max(1, Math.floor(image.naturalWidth / sourceWidth));
      const animated = tileType === "stairs" || tileType === "escalator";
      const frameIndex = animated
        ? Math.floor(sim.simTime / 3) % frameCount
        : Math.abs(visualState) % frameCount;
      const sourceHeight = image.naturalHeight > 36
        ? Math.min(FLOOR_PX, image.naturalHeight)
        : image.naturalHeight;
      const destinationHeight = sourceHeight > FLOOR_PX ? sourceHeight : FLOOR_PX;
      const destinationY = dy + FLOOR_PX - destinationHeight;
      ctx.drawImage(
        image,
        frameIndex * sourceWidth,
        0,
        sourceWidth,
        sourceHeight,
        dx,
        destinationY,
        dw,
        destinationHeight,
      );
      return;
    }
  }
  const fallback: Record<string, string> = {
    floor: "#777", lobby: "#b7b7b7", elevator: "#252525", elevatorExpress: "#202020",
    elevatorService: "#4b4b4b", stairs: "#b89b71", escalator: "#d9ad51",
  };
  ctx.fillStyle = fallback[tileType] ?? "#d7d0bd";
  ctx.fillRect(dx, dy + 2, dw, FLOOR_PX - 2);
  ctx.strokeStyle = "#555";
  ctx.strokeRect(dx + .5, dy + 2.5, dw - 1, FLOOR_PX - 3);
  if (zoom >= .75) {
    ctx.fillStyle = "#111";
    ctx.font = "9px Segoe UI";
    ctx.fillText(tileType, dx + 2, dy + 14, Math.max(0, dw - 4));
  }
}

function renderTower(): void {
  resizeCanvas();
  const ratio = window.devicePixelRatio || 1;
  const viewWidth = canvas.width / ratio;
  const viewHeight = canvas.height / ratio;
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  ctx.imageSmoothingEnabled = false;
  ctx.fillStyle = "#72bce9";
  ctx.fillRect(0, 0, viewWidth, viewHeight);
  ctx.save();
  ctx.scale(zoom, zoom);
  ctx.translate(-cameraX, -cameraY);

  const worldWidth = GRID_WIDTH * TILE_PX;
  const worldHeight = GRID_HEIGHT * FLOOR_PX;
  ctx.fillStyle = "#7ac2ec";
  ctx.fillRect(0, 0, worldWidth, (GROUND_Y + 1) * FLOOR_PX);
  ctx.fillStyle = "#715640";
  ctx.fillRect(0, (GROUND_Y + 1) * FLOOR_PX, worldWidth, worldHeight);

  const cloud = assetImage(903);
  if (cloud.complete && cloud.naturalWidth) {
    for (let x = 200; x < worldWidth; x += 950) {
      ctx.drawImage(cloud, x, Math.max(0, cameraY + 45));
    }
  }

  const left = Math.max(0, Math.floor(cameraX / TILE_PX));
  const right = Math.min(GRID_WIDTH, Math.ceil((cameraX + viewWidth / zoom) / TILE_PX));
  const top = Math.max(0, Math.floor(cameraY / FLOOR_PX));
  const bottom = Math.min(GRID_HEIGHT, Math.ceil((cameraY + viewHeight / zoom) / FLOOR_PX));
  ctx.strokeStyle = "rgb(0 0 0 / 16%)";
  ctx.lineWidth = 1 / zoom;
  for (let y = top; y <= bottom; y++) {
    const py = y * FLOOR_PX + .5;
    ctx.beginPath(); ctx.moveTo(left * TILE_PX, py); ctx.lineTo(right * TILE_PX, py); ctx.stroke();
  }

  const cells = sim.cellsToArray();
  for (const cell of cells) {
    if (!cell.isAnchor || cell.isOverlay) continue;
    if (cell.tileType === "floor" || cell.tileType === "lobby") {
      drawFacility(cell.tileType, cell.x, cell.y);
    }
  }
  for (const facility of sim.facilitiesToArray()) {
    drawFacility(
      facility.tileType,
      facility.x,
      facility.y,
      facility.width,
      resourceForFacility(facility.tileType, facility.objectTypeCode),
      facility.visualState,
    );
  }
  for (const cell of cells) {
    if (!cell.isAnchor || !cell.isOverlay) continue;
    drawFacility(cell.tileType, cell.x, cell.y);
  }

  for (const car of sim.carriersToArray()) {
    if (!car.active) continue;
    const y = (GRID_HEIGHT - 1 - car.currentFloor) * FLOOR_PX;
    const x = car.column * TILE_PX + car.carIndex * 4;
    ctx.fillStyle = car.carrierMode === 2 ? "#cc2525" : "#e9d04a";
    ctx.fillRect(x + 2, y + 5, 12, 15);
    ctx.strokeStyle = "#111";
    ctx.strokeRect(x + 2.5, y + 5.5, 11, 14);
  }

  const sims = sim.simsToArray();
  const simColors = { low: "#1458e8", medium: "#e9d600", high: "#e02121" } as const;
  for (const person of sims.slice(0, 3000)) {
    const floor = person.selectedFloor >= 0 ? person.selectedFloor : person.floorAnchor;
    const y = (GRID_HEIGHT - 1 - floor) * FLOOR_PX;
    const x = person.homeColumn * TILE_PX + 3 + (person.baseOffset % 4) * 2;
    ctx.fillStyle = simColors[person.currentTripStressLevel];
    ctx.fillRect(x, y + 11, 2, 8);
    ctx.fillRect(x - 1, y + 9, 4, 3);
  }

  const fireImage = assetImage(3944 + (Math.floor(sim.simTime / 3) % 4));
  if (fireImage.complete && fireImage.naturalWidth) {
    const { fireLeftPos, fireRightPos } = sim.fireFronts;
    for (let floor = 0; floor < fireLeftPos.length; floor++) {
      const positions = new Set([fireLeftPos[floor], fireRightPos[floor]]);
      positions.delete(0xffff);
      const y = (GRID_HEIGHT - 1 - floor) * FLOOR_PX;
      for (const position of positions) {
        ctx.drawImage(
          fireImage,
          position * TILE_PX - fireImage.naturalWidth,
          y + FLOOR_PX - fireImage.naturalHeight,
          fireImage.naturalWidth * 2,
          fireImage.naturalHeight,
        );
      }
    }
  }

  const eventPresentation = sim.eventPresentation;
  if (
    (eventPresentation.gameStateFlags & 8) !== 0 &&
    eventPresentation.helicopterExtinguishPos > 0
  ) {
    const helicopter = assetImage(3949);
    if (helicopter.complete && helicopter.naturalWidth) {
      const y = (GRID_HEIGHT - 1 - eventPresentation.fireFloor) * FLOOR_PX;
      ctx.drawImage(
        helicopter,
        eventPresentation.helicopterExtinguishPos * TILE_PX - helicopter.naturalWidth,
        y - helicopter.naturalHeight,
        helicopter.naturalWidth * 2,
        helicopter.naturalHeight,
      );
    }
  }

  ctx.fillStyle = "rgb(0 0 0 / 65%)";
  ctx.font = "11px Segoe UI";
  ctx.textAlign = "right";
  for (let y = top; y < bottom; y++) {
    const floor = yToFloor(y) - 10;
    ctx.fillText(floor === 0 ? "L" : `${floor}`, cameraX + 29, y * FLOOR_PX + 16);
  }
  ctx.textAlign = "start";
  ctx.restore();
  dirty = false;
}

function screenToCell(clientX: number, clientY: number): { x: number; y: number } {
  const rect = canvas.getBoundingClientRect();
  const worldX = cameraX + (clientX - rect.left) / zoom;
  const worldY = cameraY + (clientY - rect.top) / zoom;
  return {
    x: Math.max(0, Math.min(GRID_WIDTH - 1, Math.floor(worldX / TILE_PX))),
    y: Math.max(0, Math.min(GRID_HEIGHT - 1, Math.floor(worldY / FLOOR_PX))),
  };
}

function handleWorldCell(x: number, y: number): boolean {
  if (selectedTool === "inspect") {
    const info = sim.queryCell(x, y);
    const floor = yToFloor(y) - 10;
    setStatus(`Floor ${floor}, column ${x}: ${info.tileType}${info.objectInfo ? ` · state ${info.objectInfo.unitStatus} · evaluation ${info.objectInfo.evalLevel}` : ""}`);
    openInspector(x, y);
    return true;
  }
  const command = selectedTool === "bulldoze"
    ? { type: "remove_tile" as const, x, y }
    : { type: "place_tile" as const, x, y, tileType: selectedTool };
  const result = sim.submitCommand(command);
  if (result.accepted) {
    const definition = TOOLS.find((tool) => tool.id === selectedTool);
    setStatus(`${definition?.label ?? selectedTool} ${selectedTool === "bulldoze" ? "removed" : "built"} at floor ${yToFloor(y) - 10}, column ${x}`);
    dirty = true;
    updateHud(true);
    return true;
  } else {
    setStatus(`Cannot build: ${result.reason ?? "rejected"}`);
    return false;
  }
}

function handleWorldClick(clientX: number, clientY: number): void {
  const { x, y } = screenToCell(clientX, clientY);
  handleWorldCell(x, y);
}

function handleWorldRange(from: { x: number; y: number }, to: { x: number; y: number }): void {
  const first = Math.min(from.x, to.x);
  const last = Math.max(from.x, to.x);
  let changed = 0;
  for (let x = first; x <= last; x++) {
    if (handleWorldCell(x, from.y)) changed++;
  }
  const definition = TOOLS.find((tool) => tool.id === selectedTool);
  if (changed > 1) {
    setStatus(`${definition?.label ?? selectedTool}: ${changed} tiles changed on floor ${yToFloor(from.y) - 10}.`);
  }
}

function saveGame(): void {
  localStorage.setItem(SAVE_KEY, JSON.stringify(sim.saveState()));
  setStatus("Tower saved inside the self-contained port profile.");
}

function loadGame(): void {
  const json = localStorage.getItem(SAVE_KEY);
  if (!json) { setStatus("No saved tower is available yet."); return; }
  sim = TowerSim.fromSnapshot(JSON.parse(json));
  dirty = true;
  updateHud(true);
  setStatus("Tower loaded.");
}

function safeFilename(name: string): string {
  const stem = name.replace(/[^a-z0-9 _-]+/gi, "").trim().replace(/\s+/g, "_");
  return (stem || "Tower").slice(0, 48);
}

function downloadFile(filename: string, data: BlobPart, type: string): void {
  const url = URL.createObjectURL(new Blob([data], { type }));
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  document.body.append(link);
  link.click();
  link.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 1_000);
}

function exportGameFile(): void {
  const snapshot = sim.saveState();
  const payload = {
    format: "simtower-native-snapshot",
    version: 1,
    exportedAt: new Date().toISOString(),
    snapshot,
  };
  downloadFile(
    `${safeFilename(snapshot.world.name)}.stn.json`,
    JSON.stringify(payload),
    "application/json",
  );
  setStatus("Portable native tower exported.");
}

function exportOriginalGameFile(): void {
  try {
    const exported = exportLegacyTdt(sim.saveState());
    const buffer = exported.bytes.buffer.slice(
      exported.bytes.byteOffset,
      exported.bytes.byteOffset + exported.bytes.byteLength,
    ) as ArrayBuffer;
    downloadFile(
      `${safeFilename(sim.saveState().world.name)}.TDT`,
      buffer,
      "application/octet-stream",
    );
    const notes = exported.report.warnings.length
      ? ` ${exported.report.warnings.join(" ")}`
      : "";
    setStatus(
      `Original .TDT exported: ${exported.report.roomsWritten} room record(s), ${exported.report.elevatorsWritten} shaft(s), ${exported.report.stairsWritten} stair record(s).${notes}`,
    );
  } catch (error) {
    const message =
      error instanceof LegacyExportError || error instanceof Error
        ? error.message
        : "Unknown export error";
    setStatus(`Original .TDT export failed: ${message}`);
  }
}

function legacyImportSummary(report: LegacyTdtImportReport): string {
  const lines = [
    `Import original SimTower save “${report.filename}”?`,
    "",
    `Rating: ${report.starCount === 6 ? "TOWER" : `${report.starCount} star`}`,
    `Cash: ${formatMoney(report.cash)}`,
    `Clock: day ${report.dayCounter + 1}, tick ${report.dayTick}`,
    `Facilities: ${report.roomsPlaced} restored, ${report.roomsSkipped} skipped`,
    `Transport: ${report.elevatorsPlaced} elevator shaft(s), ${report.stairsPlaced} stair/escalator flight(s)`,
    `Population: ${report.populationImported === null ? "rebuilt from facilities" : report.populationImported.toLocaleString()}`,
  ];
  if (report.warnings.length) {
    lines.push("", "Fidelity notes:", ...report.warnings.slice(0, 12).map((warning) => `• ${warning}`));
    if (report.warnings.length > 12) lines.push(`• ${report.warnings.length - 12} additional note(s)`);
  }
  lines.push("", "The current tower will be replaced.");
  return lines.join("\n");
}

async function importGameFile(file: File): Promise<void> {
  if (file.size > 16 * 1024 * 1024) {
    setStatus("Import refused: the selected file is too large.");
    return;
  }
  try {
    const buffer = await file.arrayBuffer();
    const bytes = new Uint8Array(buffer);
    let snapshot: SimSnapshot;
    let prompt: string;
    if (looksLikeLegacyTdt(bytes) || /\.tdt$/i.test(file.name)) {
      const imported = importLegacyTdt(buffer, file.name);
      snapshot = imported.snapshot;
      prompt = legacyImportSummary(imported.report);
    } else {
      const text = new TextDecoder().decode(bytes);
      const parsed = JSON.parse(text) as
        | SimSnapshot
        | { format?: string; snapshot?: SimSnapshot };
      snapshot =
        "snapshot" in parsed && parsed.snapshot ? parsed.snapshot : parsed as SimSnapshot;
      prompt = `Import portable tower “${file.name}”?\n\nThe current tower will be replaced.`;
    }
    if (!await askConfirmation(prompt)) return;
    sim = TowerSim.fromSnapshot(snapshot);
    localStorage.setItem(SAVE_KEY, JSON.stringify(sim.saveState()));
    closeInspector();
    cameraX = 0;
    cameraY = GROUND_Y * FLOOR_PX - 520;
    dirty = true;
    updateHud(true);
    setStatus(/\.tdt$/i.test(file.name) ? "Original SimTower tower imported." : "Portable tower imported.");
  } catch (error) {
    const message =
      error instanceof LegacyImportError || error instanceof Error
        ? error.message
        : String(error);
    setStatus(`Import failed: ${message}`);
  }
}

canvas.addEventListener("contextmenu", (event) => event.preventDefault());
canvas.addEventListener("pointerdown", (event) => {
  if (event.button === 1 || event.button === 2) {
    dragging = true;
    dragButton = event.button;
    dragStartX = event.clientX;
    dragStartY = event.clientY;
    dragCameraX = cameraX;
    dragCameraY = cameraY;
    canvas.setPointerCapture(event.pointerId);
  } else if (event.button === 0 && ["floor", "lobby", "bulldoze"].includes(selectedTool)) {
    rangeBuilding = true;
    rangeStart = screenToCell(event.clientX, event.clientY);
    canvas.setPointerCapture(event.pointerId);
  }
});
canvas.addEventListener("pointermove", (event) => {
  if (rangeBuilding) {
    const end = screenToCell(event.clientX, event.clientY);
    setStatus(`${TOOLS.find((tool) => tool.id === selectedTool)?.label}: columns ${Math.min(rangeStart.x, end.x)}–${Math.max(rangeStart.x, end.x)} on floor ${yToFloor(rangeStart.y) - 10}`);
    return;
  }
  if (!dragging) return;
  cameraX = Math.max(0, dragCameraX - (event.clientX - dragStartX) / zoom);
  cameraY = Math.max(0, dragCameraY - (event.clientY - dragStartY) / zoom);
  dirty = true;
});
canvas.addEventListener("pointerup", (event) => {
  if (rangeBuilding && event.button === 0) {
    rangeBuilding = false;
    const end = screenToCell(event.clientX, event.clientY);
    canvas.releasePointerCapture(event.pointerId);
    handleWorldRange(rangeStart, end);
    return;
  }
  if (dragging && event.button === dragButton) {
    dragging = false;
    canvas.releasePointerCapture(event.pointerId);
    return;
  }
  if (event.button === 0) handleWorldClick(event.clientX, event.clientY);
});
canvas.addEventListener("wheel", (event) => {
  event.preventDefault();
  const rect = canvas.getBoundingClientRect();
  const beforeX = cameraX + (event.clientX - rect.left) / zoom;
  const beforeY = cameraY + (event.clientY - rect.top) / zoom;
  zoom = Math.max(.35, Math.min(4, zoom * (event.deltaY < 0 ? 1.15 : 1 / 1.15)));
  cameraX = Math.max(0, beforeX - (event.clientX - rect.left) / zoom);
  cameraY = Math.max(0, beforeY - (event.clientY - rect.top) / zoom);
  dirty = true;
}, { passive: false });

pauseButton.addEventListener("click", () => {
  paused = !paused;
  pauseButton.textContent = paused ? "Resume" : "Pause";
});
speedSelect.addEventListener("change", () => { speed = Number(speedSelect.value) || 1; });
soundButton.addEventListener("click", () => {
  soundEnabled = !soundEnabled;
  soundButton.textContent = soundEnabled ? "Sound On" : "Sound Off";
  soundButton.setAttribute("aria-pressed", String(soundEnabled));
});
document.querySelector("#save")?.addEventListener("click", saveGame);
document.querySelector("#load")?.addEventListener("click", loadGame);
document.querySelector("#export")?.addEventListener("click", exportGameFile);
document.querySelector("#export-tdt")?.addEventListener("click", exportOriginalGameFile);
const importFileInput = document.querySelector<HTMLInputElement>("#import-file")!;
document.querySelector("#import")?.addEventListener("click", () => {
  importFileInput.value = "";
  importFileInput.click();
});
importFileInput.addEventListener("change", () => {
  const file = importFileInput.files?.[0];
  if (file) void importGameFile(file);
});
document.querySelector("#help-button")?.addEventListener("click", openHelp);
document.querySelector("#help-close")?.addEventListener("click", closeHelp);
document.querySelector("#inspector-close")?.addEventListener("click", closeInspector);
helpSearch.addEventListener("input", () => renderHelpIndex(helpSearch.value));
document.querySelector("#new")?.addEventListener("click", async () => {
  if (!await askConfirmation("Start a new tower? The current tower remains available only if you saved it.")) return;
  sim = TowerSim.create("local", "Tower", "perfect-parity");
  dirty = true;
  updateHud(true);
  setStatus("New tower created.");
});
window.addEventListener("resize", () => { dirty = true; });
window.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && helpOpen) {
    event.preventDefault();
    closeHelp();
  } else if (event.key === "Escape" && inspectorOpen) {
    event.preventDefault();
    closeInspector();
  } else if (event.code === "Space" && !helpOpen && !confirmationOpen) {
    event.preventDefault();
    pauseButton.click();
  } else if (event.ctrlKey && event.key.toLowerCase() === "s") {
    event.preventDefault();
    saveGame();
  } else if (event.key === "F1") {
    event.preventDefault();
    openHelp();
  }
});

async function loadCatalog(): Promise<void> {
  const response = await fetch("./catalog.json");
  const catalog = await response.json() as { resources: Array<{ type_id: number | string; resource_id: number | string }> };
  existingResourceIds = new Set(
    catalog.resources
      .filter((resource) => resource.type_id === 2)
      .map((resource) => Number(resource.resource_id)),
  );
  for (const tool of TOOLS) if (tool.resource) assetImage(tool.resource);
  assetImage(903);
  for (const resourceId of [3944, 3945, 3946, 3947, 3949, 10000, 10001]) {
    assetImage(resourceId);
  }
  setStatus("Ready. Choose a construction tool, then click a supported floor.");
  dirty = true;
}

function frame(): void {
  if (!paused && !confirmationOpen && !helpOpen && !inspectorOpen) {
    const canvasRect = canvas.getBoundingClientRect();
    sim.setNewsViewport(
      Math.floor(cameraX / TILE_PX),
      Math.floor(cameraY / FLOOR_PX),
      Math.ceil(canvasRect.width / zoom / TILE_PX),
      Math.ceil(canvasRect.height / zoom / FLOOR_PX),
    );
    for (let index = 0; index < speed; index++) {
      const result = sim.step();
      if (result.cellPatches.length || result.economyChanged) dirty = true;
      for (const notification of result.notifications) {
        if (notification.message) setStatus(notification.message);
        playNotificationSound(
          notification.kind,
          notification.message,
          notification.soundResourceId,
        );
      }
      for (const prompt of result.prompts) {
        playWave(prompt.promptKind === "bomb_ransom" ? 10003 : prompt.promptKind === "fire_rescue" ? 10006 : 10014);
        const message = prompt.message + (prompt.cost ? ` Cost: ${formatMoney(prompt.cost)}.` : "");
        const imageResourceId = prompt.promptKind === "bomb_ransom"
          ? 10000
          : prompt.promptKind === "fire_rescue"
            ? 10001
            : undefined;
        void askConfirmation(message, imageResourceId).then((accepted) => {
          sim.submitCommand({ type: "prompt_response", promptId: prompt.promptId, accepted });
        });
        break;
      }
    }
    updateHud();
    if (sim.simTime % 3 === 0) dirty = true;
  }
  if (dirty) renderTower();
  requestAnimationFrame(frame);
}

buildToolbar();
updateHud(true);
void loadCatalog();
void loadHelp();
requestAnimationFrame(frame);
