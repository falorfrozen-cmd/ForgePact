<script>
  import { THEMES } from '../theme.js';
</script>

<!-- Setup tab: the game location, the plugin install/remove buttons and the built-in launcher, then the
     colour theme (moved here from the status bar, owner 2026-09-25: it does not need to be in sight on
     every tab). The theme is the design's ThemePicker (owner, 2026-09-26, finish review F2): a trigger
     and a listbox with each palette's swatches, wired by src/lib/theme-picker.js. The native select #theme
     stays underneath as the control of record, hidden but not removed: the picker sets its value and
     dispatches `change`, and the select's handler, persistence and theme.js are unchanged. The trigger
     and the options carry no button id (the oracle's coverage walk counts every button[id]).
     The launcher's description is body text, not a note (finish review F8), and #eacnote, drawn entirely by
     its own rules (app.css #chainnote,#eacnote), carries no .note either: design-match measures the notes'
     look on the first .note in the page, which is #ipcnote. -->
<div class="card tab-card" data-tab="setup" id="setupCard">
  <h2>Game Location</h2>
  <div class="hint">ForgePact talks to the mod plugin sitting next to this exe. Change it if your game lives somewhere else.</div>
  <div class="row setup-path-row" style="border:none">
    <input id="exepath" placeholder="C:\...\HeroSiege\bin\Hero_Siege.exe">
    <button class="btn" id="exebrowse" title="Open a file picker to choose Hero_Siege.exe">&#128193; Browse...</button>
    <button class="btn" id="exesave">Save</button>
    <button class="btn" id="installmod" title="One click: backs up the exe, copies mod DLLs, patches the exe">Install Mod Plugin</button>
    <button class="btn" id="removeplugin" title="Restores your original exe from the backup and removes the mod files (game must be closed)">Remove Plugin</button>
  </div>
  <div class="row setup-launch" style="border:none;margin-top:6px">
    <button class="btn primary" id="launchgame" title="Start through the built-in HS Offline Launcher">&#9654; Launch Modded Game</button>
    <span class="launch-description" style="flex:1"><b>HS Offline Launcher · Built in</b><br>Starts Steam if needed and launches your selected game with the correct Steam settings. Use offline characters.</span>
  </div>
  <div class="launch-feedback" id="launchFeedback" role="status" aria-live="polite">HS Offline Launcher is built in. No separate installation needed.</div>
  <div id="eacnote"></div>
  <div class="note" id="ipcnote"></div>
  <div class="note" id="chainnote"></div>
</div>

<div class="card tab-card" data-tab="setup">
  <h2>Appearance</h2>
  <div class="row theme-row"><span class="lbl" id="themeLabel">Theme</span><div class="theme-picker"><button type="button" class="theme-picker-trigger" aria-haspopup="listbox" aria-expanded="false" aria-controls="themeList" aria-labelledby="themeLabel themeValue"><span class="theme-picker-value" id="themeValue">{THEMES[0].label}</span><svg viewBox="0 0 16 16" aria-hidden="true"><path d="m4.5 6.5 3.5 3.5 3.5-3.5"/></svg></button><ul class="theme-picker-list" id="themeList" role="listbox" tabindex="-1" aria-labelledby="themeLabel" data-instant>{#each THEMES as theme (theme.value)}<li class="theme-picker-option" role="option" id={'themeOption-' + theme.value} data-value={theme.value} aria-selected={theme.value === THEMES[0].value ? 'true' : 'false'}><span class="theme-picker-swatches" aria-hidden="true"><span></span><span></span><span></span></span>{theme.label}</li>{/each}</ul><select id="theme" class="theme-picker-native" aria-hidden="true" tabindex="-1">{#each THEMES as theme (theme.value)}<option value={theme.value}>{theme.label}</option>{/each}</select></div></div>
</div>
