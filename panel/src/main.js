// Mount the page, then run the script that used to sit at the end of its
// <body>: the markup has to exist before panel.js looks anything up.
// tokens.css (generated from the Figma export) comes first, so fonts.css and
// app.css can use its custom properties.
import './tokens.css';
import './fonts.css';
import './app.css';
import './ember/palette.css';
import './ember/ember.css';
import './ember/relief.css';
import './ember/compat.css';
import { initEmberShell } from './ember/ember.js';
import { mount } from 'svelte';
import App from './App.svelte';
import { start } from './panel.js';
import { installEnabledModsForm } from './lib/enabled-mods-form.js';
import { installEnabledModsUndo } from './lib/enabled-mods-undo.js';
import { installRememberedValues } from './lib/remembered-value.js';
import { installReviewFixes } from './lib/review-fixes.js';
import { installPluginWarning } from './lib/plugin-warning.js';
import { installSliderNotes } from './lib/slider-note.js';
import { installThemePicker } from './lib/theme-picker.js';

mount(App, { target: document.getElementById('app') });
// The restyle's additions only watch the DOM panel.js produces, so they go in
// before start(): boot()'s first render of the list and rows is already seen.
initEmberShell();
const form = installEnabledModsForm();
installEnabledModsUndo(form);
installRememberedValues();
installReviewFixes();
installPluginWarning();
installSliderNotes();
installThemePicker();
start();
