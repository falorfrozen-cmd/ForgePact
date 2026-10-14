// Rarity and zone alerts (#185): the main window plays each one once, after
// claiming it through client.announce(). A stub until its owner, the `sounds`
// item, replaces it: startAlerts(client) does nothing and returns its stop
// function, so Tracker.svelte can start and stop it already.
export function startAlerts(client) {
  return function stopAlerts() {};
}
