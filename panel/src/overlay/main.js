// The Tracker's overlay window (#185): the second page the panel builds
// (overlay.html), which src/tracker_overlay.py opens in a small frameless
// window above the game. It takes the panel's tokens and fonts and the
// palette the player picked, and none of the panel's shell: no Ember
// artwork, no tabs, no settings. Overlay.svelte is the whole page.
import '../tokens.css';
import '../fonts.css';
import '../ember/palette.css';
import './overlay.css';
import { mount } from 'svelte';
import { applyTheme } from '../theme.js';
import Overlay from './Overlay.svelte';

// The default palette straight away, so the window never paints unthemed
// while `/api/state` answers; Overlay.svelte paints the saved one.
applyTheme(null);
mount(Overlay, { target: document.getElementById('overlay') });
