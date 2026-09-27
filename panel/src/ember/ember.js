import { ST } from '../state.svelte.js';
import { activeTab, openTab, openModsSubtab } from '../nav.js';
import { toast } from '../panel.js';
import { switchControlId } from '../enabled-mods.js';

// Presentation only. All writes go through the existing controls and serialized API.
const EMBER_QUICK = {
  density: {
    label: "Monster density",
    hint: "Applies in a new zone.",
    icon: "skull",
    selector: "#den",
  },
  magicfind: {
    label: "Magic Find",
    hint: "Increase your current total.",
    icon: "gem",
    selector: 'input[data-sec="stats"][data-key="magicfind"]',
  },
  mining_ore: {
    label: "Mining ore",
    hint: "More ore from each vein.",
    icon: "ore",
    selector: 'input[data-sec="drops"][data-key="mining_ore"]',
  },
  gold: {
    label: "Gold",
    hint: "Multiply gold drops.",
    icon: "gem",
    selector: 'input[data-sec="drops"][data-key="gold"]',
  },
  exp: {
    label: "Experience",
    hint: "Multiply experience gained.",
    icon: "banner",
    selector: 'input[data-sec="stats"][data-key="exp"]',
  },
  movespeed: {
    label: "Movement speed",
    hint: "Multiply your movement speed.",
    icon: "paw",
    selector: 'input[data-sec="stats"][data-key="movespeed"]',
  },
};
let emberQuickKeys = ["density", "magicfind", "mining_ore"],
  emberSearchEntries = [];
const emberEscape = (s) =>
  String(s).replace(
    /[&<>"']/g,
    (c) =>
      ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[
        c
      ],
  );
function emberNavButton(name, label, path) {
  const button = document.createElement("button");
  button.type = "button";
  button.className = "tabbtn";
  button.id = "nav-" + name;
  button.dataset.tab = name;
  button.setAttribute("role", "tab");
  button.setAttribute("aria-controls", "workspace");
  button.innerHTML = `<svg viewBox="0 0 24 24" aria-hidden="true"><path d="${path}"/></svg>${label}`;
  return button;
}
export function initEmberShell() {
  const homes = new Map();
  const keepHome = (el) => {
    const anchor = document.createComment('Ember: original position');
    el.before(anchor); homes.set(el, anchor); return el;
  };
  const nav = document.querySelector(".tabbar");
  keepHome(document.getElementById('nav-setup'));
  for (const selector of ['#saveIndicator', '#chipApply', '.page-actions', '.status-foot'])
    keepHome(document.querySelector(selector));
  nav.prepend(
    emberNavButton(
      "overview",
      "Overview",
      "M3 11 12 3l9 8M5 10v11h5v-7h4v7h5V10",
    ),
  );
  const char = document.getElementById("nav-modifiers");
  char.lastChild.textContent = "Character";
  nav.append(
    document.getElementById("nav-setup"),
    emberNavButton(
      "help",
      "Help",
      "M3 4c4-1 6 0 9 2 3-2 5-3 9-2v16c-4-1-6 0-9 2-3-2-5-3-9-2ZM12 6v16",
    ),
  );
  document.querySelector(".brand-sub").textContent = "HERO SIEGE · OFFLINE";
  document.querySelector(".brand-name").textContent = "ForgePact";
  const plugin = document.createElement("span");
  plugin.id = "emberPluginStatus";
  plugin.className = "chip off";
  plugin.textContent = "Checking plugin…";
  document.getElementById("statusbar").append(plugin);
  const search = document.createElement("button");
  search.type = "button";
  search.className = "ember-search-button";
  search.dataset.emberSearch = "";
  search.setAttribute("aria-label", "Find a setting");
  search.innerHTML = "Find a setting… <kbd>Ctrl K</kbd>";
  document.querySelector(".topline").append(search);
  const footer = document.createElement("footer");
  footer.className = "ember-footer";
  footer.setAttribute("aria-label", "Settings actions");
  const saved = document.createElement("div");
  saved.className = "ember-save";
  const saveCopy = document.createElement("div");
  saveCopy.className = "ember-save-copy";
  saveCopy.append(
    document.getElementById("saveIndicator"),
    document.getElementById("chipApply"),
  );
  saved.append(saveCopy);
  footer.append(saved, document.querySelector(".page-actions"));
  document.body.append(footer);
  const credits = document.querySelector('.status-foot');
  credits.classList.add('sidebar-foot');
  const applyThemeLayout = () => {
    const ember = document.documentElement.dataset.theme === 'ember';
    for (const el of [footer, search, plugin, document.getElementById('nav-overview'), document.getElementById('nav-help')]) el.hidden = !ember;
    if (ember) {
      nav.append(document.getElementById('nav-setup'), document.getElementById('nav-help'));
      saveCopy.append(document.getElementById('saveIndicator'), document.getElementById('chipApply'));
      footer.append(document.querySelector('.page-actions'));
      document.querySelector('.sidebar').append(credits);
    } else {
      for (const [el, anchor] of homes) anchor.after(el);
      for (const id of ['emberSearchDialog','emberQuickDialog']) document.getElementById(id).close();
      if (['overview', 'help'].includes(activeTab)) openTab('modifiers');
    }
    nav.setAttribute('aria-orientation', ember && matchMedia('(min-width: 721px)').matches ? 'vertical' : 'horizontal');
    syncEmberNavigation(activeTab);
  };
  document.addEventListener('forgepact:theme', applyThemeLayout);
  matchMedia('(min-width: 721px)').addEventListener('change', applyThemeLayout);
  document.addEventListener('forgepact:ready', () => { prepareEmber(); applyThemeLayout(); });
  document.addEventListener('forgepact:navigate', () => syncEmberNavigation(activeTab));
  document.addEventListener('forgepact:settings', syncEmber);
  document.addEventListener('forgepact:status', updateEmberStatus);
  applyThemeLayout();
  const apply = document.getElementById("applyall");
  apply.textContent = "Apply now";
  apply.title =
    "Apply all enabled settings now, or queue them if the game is closed.";
  const auto = document.querySelector(".auto-control");
  auto.childNodes[0].textContent = "Auto-apply on launch ";
  // Keep native <dialog> focus trapping and Escape handling; no document-wide focus hacks.
  document
    .querySelectorAll("[data-close-dialog]")
    .forEach((b) => (b.onclick = () => b.closest("dialog").close()));
  document
    .querySelectorAll("[data-ember-search]")
    .forEach((b) => (b.onclick = openEmberSearch));
  document
    .querySelectorAll("[data-open-tab]")
    .forEach((b) => (b.onclick = () => openTab(b.dataset.openTab)));
  document.getElementById("overviewEditPool").onclick = () => {
    openTab("world");
    const target = document.getElementById("satanicMods");
    target.scrollIntoView({ block: "start" });
    document.getElementById("satSearch").focus({ preventScroll: true });
  };
  document.getElementById("emberSearchInput").oninput = renderEmberSearch;
  document.getElementById("emberSearchInput").onkeydown = (e) => {
    if (e.key === "ArrowDown") {
      e.preventDefault();
      document.querySelector("#emberSearchResults button")?.focus();
    }
  };
  document.addEventListener("keydown", (e) => {
    if (document.documentElement.dataset.theme === 'ember' && (e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "k") {
      e.preventDefault();
      openEmberSearch();
    }
  });
  document.getElementById("customizeQuick").onclick = openEmberQuickChoices;
  document.getElementById("saveQuickChoices").onclick = () => {
    const keys = [
      ...document.querySelectorAll("#emberQuickChoices input:checked"),
    ].map((b) => b.value);
    if (!keys.length || keys.length > 3) {
      document.getElementById("emberQuickMessage").textContent =
        "Choose between one and three settings.";
      return;
    }
    emberQuickKeys = keys;
    try {
      localStorage.setItem("forgepact_quick_controls", JSON.stringify(keys));
    } catch (_) {}
    renderEmberQuick();
    syncEmber();
    document.getElementById("emberQuickDialog").close();
  };
  try {
    const keys = JSON.parse(localStorage.getItem("forgepact_quick_controls"));
    if (
      Array.isArray(keys) &&
      keys.length &&
      keys.length <= 3 &&
      new Set(keys).size === keys.length &&
      keys.every((k) => Object.hasOwn(EMBER_QUICK, k))
    )
      emberQuickKeys = keys;
  } catch (_) {}
  document.querySelectorAll("[data-ember-toggle]").forEach(
    (box) =>
      (box.onchange = async () => {
        const original = document.getElementById(box.dataset.emberToggle);
        if (!original || original.disabled || !ST?.cfg) {
          syncEmber();
          return;
        }
        original.checked = box.checked;
        await original.onchange({ target: original });
        syncEmber();
      }),
  );
}
function syncEmberNavigation(name) {
  document.body.dataset.emberTab = name;
  if (name === "overview") syncEmber();
}
function prepareEmber() {
  renderEmberQuick();
  syncEmber();
  updateEmberStatus();
}
function quickSwitch(key) {
  if (key === 'density') return document.getElementById('den_on');
  const source = document.querySelector(EMBER_QUICK[key].selector);
  return document.getElementById(switchControlId(`${source.dataset.sec}.${source.dataset.key}`));
}
function renderEmberQuick() {
  const target = document.getElementById("overviewQuick");
  target.innerHTML = emberQuickKeys
    .map((key) => {
      const m = EMBER_QUICK[key],
        source = document.querySelector(m.selector);
      if (!source) return "";
      const enable = `<label class="ember-density-enable"><input type="checkbox" data-quick-enable="${key}" aria-label="Enable ${m.label} (quick control)"> <span data-quick-state="${key}">Off</span></label>`;
      return `<div class="ember-quick-row" data-quick-row="${key}"><span class="ember-item-icon ember-${m.icon}" aria-hidden="true"></span><div class="ember-control-label"><label for="quick-number-${key}">${m.label}</label><small>${m.hint}</small>${enable}</div><input type="range" class="ember-quick-range" data-quick-range="${key}" min="${source.min}" max="${source.max}" step="any" aria-label="${m.label} slider (quick control)"><div class="ember-number"><button type="button" data-quick-step="${key}" data-direction="-1" aria-label="Decrease ${m.label} (quick control)">−</button><input type="number" id="quick-number-${key}" data-quick-number="${key}" min="${source.min}" max="${source.max}" step="any" aria-label="${m.label} multiplier (quick control)"><button type="button" data-quick-step="${key}" data-direction="1" aria-label="Increase ${m.label} (quick control)">+</button></div></div>`;
    })
    .join("");
  target.setAttribute("aria-busy", "false");
  target.querySelectorAll("[data-quick-range]").forEach((range) => {
    range.oninput = () => {
      const number = document.getElementById(
        "quick-number-" + range.dataset.quickRange,
      );
      number.value = range.value;
      paintEmberRange(range);
    };
    range.onchange = () =>
      commitEmberNumber(range.dataset.quickRange, range.value, false);
  });
  target.querySelectorAll("[data-quick-number]").forEach((number) => {
    number.onchange = () =>
      commitEmberNumber(number.dataset.quickNumber, number.value, true);
    number.onkeydown = (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        number.blur();
      } else if (e.key === "Escape") {
        e.preventDefault();
        number.value = document.querySelector(
          EMBER_QUICK[number.dataset.quickNumber].selector,
        ).value;
        number.blur();
        syncEmber();
      }
    };
  });
  target.querySelectorAll("[data-quick-step]").forEach(
    (button) =>
      (button.onclick = () => {
        const key = button.dataset.quickStep,
          source = document.querySelector(EMBER_QUICK[key].selector),
          step = Number(source.dataset.step0) || Number(source.step) || 1;
        return commitEmberNumber(
          key,
          Number(source.value) + Number(button.dataset.direction) * step,
          true,
        );
      }),
  );
  target.querySelectorAll('[data-quick-enable]').forEach(enabled => {
    enabled.onchange = async () => {
      const source = quickSwitch(enabled.dataset.quickEnable);
      source.checked = enabled.checked;
      await source.onchange({ target: source });
      syncEmber();
    };
  });
}
async function commitEmberNumber(key, raw, typed) {
  const source = document.querySelector(EMBER_QUICK[key].selector),
    n = Number(raw);
  if (!ST?.cfg) return;
  if (raw === "" || !Number.isFinite(n)) {
    document.getElementById("quick-number-" + key).value = source.value;
    syncEmber();
    return;
  }
  const previousTyped = source.dataset.typed;
  source.step = "any";
  source.value = Math.max(
    Number(source.min),
    Math.min(Number(source.max), +n.toFixed(2)),
  );
  if (typed) source.dataset.typed = "1";
  else delete source.dataset.typed;
  try {
    source.oninput();
    const actual = Number(source.value),
      input = document.getElementById("quick-number-" + key),
      range = document.querySelector(`[data-quick-range="${key}"]`);
    if (input) input.value = actual;
    await source.onchange();
    // j() updates authoritative state before scheduling the original controls'
    // repaint. Read that state even while this quick editor retains focus.
    const saved =
      key === "density"
        ? ST.cfg.density
        : ST.cfg[source.dataset.sec][source.dataset.key];
    if (input && Number(input.value) === actual) input.value = saved;
    if (range && Number(range.value) === Number(raw)) {
      range.value = saved;
      paintEmberRange(range);
    }
  } finally {
    if (previousTyped === undefined) delete source.dataset.typed;
    else source.dataset.typed = previousTyped;
    syncEmber();
  }
}
function paintEmberRange(range) {
  const pct =
    (100 * (Number(range.value) - Number(range.min))) /
    (Number(range.max) - Number(range.min));
  range.style.background = `linear-gradient(to right,#e99345 ${pct}%,#33363b ${pct}%)`;
}
function syncEmber() {
  if (!ST?.cfg) return;
  for (const key of emberQuickKeys) {
    const source = document.querySelector(EMBER_QUICK[key].selector),
      range = document.querySelector(`[data-quick-range="${key}"]`),
      number = document.getElementById("quick-number-" + key);
    if (!source || !range || !number) continue;
    if (document.activeElement !== range) {
      range.value = source.value;
      paintEmberRange(range);
    }
    if (document.activeElement !== number) number.value = source.value;
    document
      .querySelectorAll(`[data-quick-step="${key}"]`)
      .forEach(
        (b) =>
          (b.disabled =
            Number(b.dataset.direction) < 0
              ? +source.value <= +source.min
              : +source.value >= +source.max),
      );
  }
  document.querySelectorAll('[data-quick-enable]').forEach(box => {
    const original = quickSwitch(box.dataset.quickEnable);
    box.checked = original.checked;
    const value = Number(document.querySelector(EMBER_QUICK[box.dataset.quickEnable].selector).value);
    document.querySelector(`[data-quick-state="${box.dataset.quickEnable}"]`).textContent = box.checked ? (value === 1 ? 'Enabled · normal rate' : 'Enabled') : 'Off — value kept';
    box.closest('.ember-quick-row').classList.toggle('ember-quick-off', !box.checked);
  });
  document.querySelectorAll("[data-ember-toggle]").forEach((box) => {
    const original = document.getElementById(box.dataset.emberToggle);
    box.checked = original.checked;
    box.disabled = original.disabled;
    document.querySelector(`[data-state-for="${box.id}"]`).textContent =
      box.disabled ? "—" : box.checked ? "On" : "Off";
  });
  document.getElementById("quick-packs-hint").textContent =
    document.getElementById("map_reveal_packs").disabled
      ? "Enable Reveal full map first."
      : "Marks packs without spawning them.";
  for (const [polarity, id, label, floorKey, listKey] of [
    ["buff", "buffs", "Positive", "minEnabledSatanicBuffs", "satanicBuffs"],
    [
      "debuff",
      "debuffs",
      "Negative",
      "minEnabledSatanicDebuffs",
      "satanicDebuffs",
    ],
  ]) {
    const list = ST[listKey] || [],
      pool = ST.cfg.satanic_mods?.[polarity] || {},
      count = list.filter(([key]) => pool[String(key)] !== false).length,
      floor = ST[floorKey] || (polarity === "buff" ? 3 : 2);
    const el = document.getElementById("quick-" + id);
    el.textContent = list.length
      ? `${label}: ${count} selected`
      : `${label}: unavailable`;
    el.parentElement.classList.toggle("invalid", !list.length || count < floor);
    document.getElementById("quick-" + id + "-min").textContent =
      "Minimum " + floor;
  }
}
function updateEmberStatus() {
  if (!ST) return;
  const game = document.getElementById("chipGame"),
    plugin = document.getElementById("emberPluginStatus");
  if (!plugin) return;

  const ch = ST.chain || {},
    installed = ch.patched && ch.aurieCore && ch.yytk && ch.plugin;
  // Existing state proves files are installed, not a current-session heartbeat.
  // Never misrepresent an old modstate file as a live plugin connection.
  const text = installed
    ? "Plugin installed"
    : ch.exeExists
      ? "Plugin missing"
      : "Setup needed";
  if (plugin.textContent !== text) plugin.textContent = text;
  const cls = 'chip ' + (installed ? 'on' : 'warn');
  if (plugin.className !== cls) plugin.className = cls;
  const title = installed
    ? "Required plugin files are present. This is not a live connection check."
    : "Choose your game and install its plugin in Setup.";
  if (plugin.title !== title) plugin.title = title;
}
function setEmberDisconnected() {
  for (const id of ["chipGame", "emberPluginStatus"]) {
    const el = document.getElementById(id);
    if (el) {
      el.textContent =
        id === "chipGame" ? "Panel disconnected" : "Status unknown";
      el.className = "chip warn";
    }
  }
}
function openEmberQuickChoices() {
  const box = document.getElementById("emberQuickChoices");
  box.innerHTML = Object.entries(EMBER_QUICK)
    .map(
      ([key, m]) =>
        `<label><input type="checkbox" value="${key}" ${emberQuickKeys.includes(key) ? "checked" : ""}>${m.label}</label>`,
    )
    .join("");
  document.getElementById("emberQuickMessage").textContent =
    "Shortcuts stay on this device. Game settings are unchanged.";
  document.getElementById("emberQuickDialog").showModal();
}
function indexEmberSearch() {
  emberSearchEntries = [];
  for (const card of document.querySelectorAll(".tab-card[data-tab]")) {
    const tab = card.dataset.tab;
    if (["overview", "help"].includes(tab)) continue;
    for (const input of card.querySelectorAll(
      "input[type=range],.feature-card input[type=checkbox],.style-select",
    )) {
      const row = input.closest(".row"),
        label =
          row
            ?.querySelector(".label-copy,.lbl")
            ?.textContent.trim()
            .split("\n")[0] ||
          input.getAttribute("aria-label") ||
          input.id;
      if (!label) continue;
      const note = row?.nextElementSibling?.matches(".note")
        ? row.nextElementSibling.textContent
        : "";
      emberSearchEntries.push({
        label: input.id === "den" ? "Monster density" : label,
        tab,
        card: card.id,
        input,
        text: (
          label +
          " " +
          note +
          " " +
          (row?.querySelector(".feature-description")?.textContent || "")
        ).toLowerCase(),
      });
    }
  }
  const gemSearch = document.getElementById('gemfilter_toggle');
  if (gemSearch) emberSearchEntries.push({ label: 'Gems of Incarnation filter', text: 'gems incarnation mythic affix filter max roll', tab: 'loot', card: gemSearch.closest('.tab-card').id, input: gemSearch });
  emberSearchEntries.push({
    label: "Satanic Zone Mods",
    text: "satanic zone mods positive negative pool buffs debuffs",
    tab: "world",
    card: "satanicMods",
    input: document.getElementById("satSearch"),
  });
  emberSearchEntries.push({
    label: "Game location and plugin installation",
    text: "game location path plugin install launch setup",
    tab: "setup",
    card: "setupCard",
    input: document.getElementById("exepath"),
  });
}
function openEmberSearch() {
  if (!ST?.cfg) {
    toast("Settings are still loading. Try again in a moment.");
    return;
  }
  indexEmberSearch();
  const dialog = document.getElementById("emberSearchDialog");
  document.getElementById("emberSearchInput").value = "";
  renderEmberSearch();
  if (!dialog.open) dialog.showModal();
  document.getElementById("emberSearchInput").focus();
}
function renderEmberSearch() {
  const query = document
      .getElementById("emberSearchInput")
      .value.trim()
      .toLowerCase(),
    hits = emberSearchEntries.filter((e) => e.text.includes(query));
  document.getElementById("emberSearchCount").textContent = hits.length
    ? `${hits.length} settings found`
    : "No settings found. Try another name or effect.";
  const box = document.getElementById("emberSearchResults");
  box.replaceChildren();
  for (const hit of hits) {
    const b = document.createElement("button");
    b.type = "button";
    b.innerHTML = `<span>${emberEscape(hit.label)}</span><small>${emberEscape(hit.tab === "modifiers" ? "Character" : hit.tab[0].toUpperCase() + hit.tab.slice(1))} →</small>`;
    b.onclick = () => {
      document.getElementById("emberSearchDialog").close();
      openTab(hit.tab);
      if (hit.tab === "mods") openModsSubtab(hit.card);
      const row = hit.input.closest(".row") || hit.input;
      row.scrollIntoView({ block: "center" });
      hit.input.focus({ preventScroll: true });
      row.classList.add("ember-target");
      setTimeout(() => row.classList.remove("ember-target"), 1800);
    };
    box.append(b);
  }
}
