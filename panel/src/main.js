// Mount the page, then run the script that used to sit at the end of its
// <body>: the markup has to exist before panel.js looks anything up.
import './fonts.css';
import './app.css';
import { mount } from 'svelte';
import App from './App.svelte';
import { start } from './panel.js';

mount(App, { target: document.getElementById('app') });
start();
