<script>
  // One alert sound's row in the Alerts section (#185): a rarity's or the zone
  // rotation's switch, volume and Test, and under it the sound's source with
  // Choose a file and Use the bundled sound (design/tracker/tab.html, the
  // panel's .setting-entry with a .switch.slider-switch, as Monster Rarity's).
  //
  // `write(edit)` is AlertSettings' queued read-modify-write of the whole
  // settings document; the edits are settings/edit.js's. The volume writes
  // when a drag ends (or a step button is pressed), never on every pixel of
  // one. A chosen file goes to `POST /api/tracker/sound/<key>` as base64; the
  // store checks its kind and first bytes, and a refusal changes nothing and
  // is shown through `onerror`. Test plays through audio.js's mixer, unlocking
  // the page's AudioContext with the click it comes from.
  import { alertOn, setAlertOn, setVolume, volumePercent } from './edit.js';
  import { SOUND_ACCEPT, fileUpload, soundExt, soundSource } from './transfer.js';
  import { play, soundUrl, unlock } from '../audio.js';

  let { client, key, label, rarityClass = null, write, onerror } = $props();

  const STEP = 5;

  const doc = $derived(client.settings);
  const on = $derived(alertOn(doc, key));
  const saved = $derived(volumePercent(doc?.[key]));
  let dragging = $state(null);
  const pct = $derived(dragging ?? saved);
  const source = $derived(soundSource(key, client.sounds));
  let busy = $state(false);
  let fileInput = $state(null);

  async function switchTo(box) {
    const res = await write((d) => setAlertOn(d, key, box.checked));
    // A refused write leaves the switch as the document has it.
    if (res.err) { box.checked = on; onerror(res.err); }
  }

  async function writeVolume(value) {
    const res = await write((d) => setVolume(d, key, value / 100));
    dragging = null;
    if (res.err) onerror(res.err);
  }

  function step(direction) {
    const next = Math.min(100, Math.max(0, pct + direction * STEP));
    dragging = next;
    writeVolume(next);
  }

  async function test() {
    await unlock();
    void play(soundUrl(key, client.sounds), pct / 100, key);
  }

  async function chosen(e) {
    const file = e.currentTarget.files?.[0];
    e.currentTarget.value = '';
    if (!file) return;
    if (!soundExt(file.name)) {
      onerror(`${file.name} is not a WAV, MP3, OGG or FLAC file, so the ${label} sound did not change.`);
      return;
    }
    busy = true;
    try {
      const res = await client.changeSound(key, await fileUpload(file));
      if (res.err) onerror(`${file.name} was not used: ${res.err}. The ${label} sound did not change.`);
      else onerror(null);
    } finally {
      busy = false;
    }
  }

  async function useBundled() {
    busy = true;
    try {
      const res = await client.changeSound(key, { clear: true });
      onerror(res.err ? `The ${label} sound could not be reset: ${res.err}` : null);
    } finally {
      busy = false;
    }
  }
</script>

<div class="setting-entry trk-alert" data-key={key}>
  <div class="row">
    <span class="lbl">{#if rarityClass}<span class={rarityClass}>{label}</span>{:else}{label}{/if}</span>
    <label class="switch slider-switch" data-live={on ? '' : undefined}><input type="checkbox" class="trk-alert-on" checked={on} aria-label={label + ' alert'} onchange={(e) => switchTo(e.currentTarget)}><span class="sl"></span></label>
    <div class="range-control">
      <input type="range" class="trk-volume" min="0" max="100" step="1" value={pct} aria-label={label + ' volume'}
        oninput={(e) => (dragging = Number(e.currentTarget.value))} onchange={(e) => writeVolume(Number(e.currentTarget.value))}>
      <div class="value-stepper">
        <button type="button" class="step-button" data-step="-1" aria-label={'Quieter ' + label} onclick={() => step(-1)}>−</button>
        <span class="val" class:off={!on}>{on ? pct + '%' : 'off'}</span>
        <button type="button" class="step-button" data-step="1" aria-label={'Louder ' + label} onclick={() => step(1)}>+</button>
      </div>
    </div>
    <button type="button" class="trk-quiet trk-test" onclick={test}><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M5 3.5v9l7-4.5Z"/></svg>Test</button>
  </div>
  <p class="note trk-sound-src">Sound: <b>{source ?? 'bundled'}</b>
    <button type="button" class="trk-quiet" disabled={busy} onclick={() => fileInput?.click()}>Choose a file…</button>
    {#if source}<button type="button" class="trk-quiet trk-sound-clear" disabled={busy} onclick={useBundled}>Use the bundled sound</button>{/if}
    <input type="file" class="trk-sound-file visually-hidden" accept={SOUND_ACCEPT} tabindex="-1" aria-label={'Choose a sound file for ' + label} bind:this={fileInput} onchange={chosen}>
  </p>
</div>

<style>
  .trk-alert .row { flex-wrap: nowrap; }
  .trk-sound-src { display: flex; align-items: center; gap: var(--space-4); margin: 0; padding-left: calc(168px + var(--space-7)); color: var(--color-text-muted); }
  .trk-sound-src b { color: var(--color-text-primary); font-family: var(--font-mono-family), Consolas, monospace; font-weight: var(--font-weight-regular); }
  @media (max-width: 1100px) {
    .trk-sound-src { padding-left: calc(152px + var(--space-7)); }
  }
</style>
