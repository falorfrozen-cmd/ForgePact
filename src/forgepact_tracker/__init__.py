"""ForgePact's tracker: HS Offline Tracker's ledger, store and import (#185).

The plugin's `tracker` mod writes protocol-v1 session files under
`<game>\bin\bp_ipc\tracker\`; this package reads them read-only and keeps
the Tracker's totals, journal, runs and settings under
`%LOCALAPPDATA%\Hero_Siege\tracker\`. Standard library and `hs_game_sdk`
only, so nothing new has to be bundled.

- `protocol`: one line of protocol v1 to a record.
- `items`, `item_table`: the Tracker's item and room tables (generated).
- `ledger`: one session's totals, journal, zone rotations and runs.
- `store`: the tracker's own files, written whole; the one-time import.
- `service`: session-file ingest and finalization behind one lock.
"""
