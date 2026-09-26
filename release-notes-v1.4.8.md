# ForgePact 1.4.8

The panel no longer opens as a blank window when its usual ports are busy.

## Fixed

- **Blank panel window when ForgePact's ports were all in use.** ForgePact
  tries five ports of its own (8780, 8801, 8899, 9133 and 9777). When all five
  were taken, it let Windows pick any free port, and Windows could pick one the
  panel window is not allowed to open, such as 6000 or 10080. The window then
  stayed blank. ForgePact now skips those ports and asks for another. If it
  still cannot find one, it tells you so instead of opening an empty window.
  This was rare: it needs all five ports busy, and a PC whose free-port range
  has been changed from Windows' default.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. This fix is in the panel itself, so there is no need to
press **Install Mod Plugin** for it.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
