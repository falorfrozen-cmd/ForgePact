<script>
  import { THEMES } from '../theme.js';
</script>

<!-- Setup tab: the game location, the plugin install/remove buttons and the built-in launcher, then the
     colour theme (moved here from the status bar, owner 2026-09-25: it does not need to be in sight on
     every tab). The select's handler, persistence and theme.js are unchanged; it is a panel setting. -->
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
    <span class="note" style="flex:1"><b>HS Offline Launcher · Built in</b><br>Starts Steam if needed and launches your selected game with the correct Steam settings. Use offline characters.</span>
  </div>
  <div class="launch-feedback" id="launchFeedback" role="status" aria-live="polite">HS Offline Launcher is built in. No separate installation needed.</div>
  <div class="note" id="eacnote"></div>
  <div class="note" id="ipcnote"></div>
  <div class="note" id="chainnote"></div>
</div>

<div class="card tab-card" data-tab="setup">
  <h2>Appearance</h2>
  <div class="row theme-row"><label class="lbl" for="theme">Theme</label><select id="theme">{#each THEMES as theme (theme.value)}<option value={theme.value}>{theme.label}</option>{/each}</select></div>
</div>
