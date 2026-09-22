"""Original, reusable ForgePact UI artwork. No game files or network required.

The panel embeds one SVG symbol sheet; instances only reference a symbol ID.
Run `py src/panel_icons.py <output-directory>` for individual SVGs and a gallery.
Names in the mappings are panel setting keys, never localized label guesses.
"""
import argparse
import html
import json
from pathlib import Path


METAL = '#cfa477'
DARK = '#241b16'
GOLD = '#f3c462'
BLUE = '#8ccde8'
VIOLET = '#bd9fe4'
RED = '#ed9c99'
GREEN = '#99c9a9'


def path(d, fill='none', stroke=METAL, **attrs):
    extra = ''.join(f' {k.replace("_", "-")}="{v}"' for k, v in attrs.items())
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}"{extra}/>'


def circle(cx, cy, r, fill='none', stroke=METAL):
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}" stroke="{stroke}"/>'


def star(x=16, y=16, color=GOLD):
    return path(f'M{x} {y-7}l2 5 5 2-5 2-2 5-2-5-5-2 5-2Z', color)


def gem(color=BLUE):
    return path('M9 6h14l6 8-13 15L3 14Z', DARK)+path('M9 6h14l6 8-13 15L3 14Z', color)+path('m9 6 7 23 7-23M3 14h26', 'none', '#f4dcc0', opacity='.65')


def key(color=GOLD):
    return circle(12, 10, 6, DARK, color)+circle(12, 10, 2, color, color)+path('M16 15 27 26l-3 3-4-4 2-2-3-3-2 2-4-4', DARK, color)


def shield(color=BLUE, cracked=False):
    return path('m16 3 11 4v9c0 6-5 10-11 13C10 26 5 22 5 16V7Z', '#28343a', color)+path('m16 6 7 3v7c0 4-3 7-7 10Z', '#405660', color)+(path('m18 5-5 8 7 4-6 10','none',RED) if cracked else path('M16 7v16M9 12h14','none',color))


def heart(color=RED):
    return path('M16 28 5 17C-1 9 7 1 16 9 25 1 33 9 27 17Z', '#533238', color)+path('M7 15h5l3-5 3 11 3-6h5','none',color)


def drop(color=BLUE):
    return path('M16 3C13 10 6 15 6 20a10 10 0 0 0 20 0c0-5-7-10-10-17Z', '#283e52', color)+path('M11 19c-1 4 1 6 4 6','none',color)


def sword(color=RED):
    return path('m22 3 7-1-1 7-14 14-5-5Z', '#ddd0bb')+path('M25 6 12 19','none',color)+path('m7 17 9 9-3 2-9-9Z', '#9d6b45')+path('m8 23-6 6 3 2 6-6', DARK)


def skull(color=METAL):
    return path('M7 22C0 14 5 4 16 4s16 10 9 18l-3 1v6H10v-6Z', '#332823',color)+path('m9 14 5 2-1 4H8Zm14 0-5 2 1 4h5ZM16 20l-2 3h4ZM14 25v4m4-4v4', DARK,color)


def boot():
    return path('M10 3h12l-2 15 8 4v7H5v-9l5-4Z', '#544033')+path('M10 8h11M9 13h11M5 25h23M10 20l4 2', 'none', GOLD)


def portal(color=VIOLET):
    return path('M4 28V14a12 12 0 0 1 24 0v14h-5V14a7 7 0 0 0-14 0v14Z', '#44332b')+path('M10 27V14a6 6 0 0 1 12 0v13Z', '#29213f',color)+path('M18 12c-7-1-8 10-3 10 5 0 5-7 1-6M2 28h9m10 0h9','none',color)


def orb(color=VIOLET):
    return circle(16,15,10,'#322842',color)+path('m16 6-3 8 6 4-3 5M7 24l-2 5h22l-2-5',DARK,color)+circle(11,10,2,color,color)


def chest():
    return path('M4 14V9c0-8 24-8 24 0v5ZM3 14h26v14H3Z','#5a3d28')+path('M9 6v22M23 6v22M3 14h26', 'none',GOLD)+path('M13 13h6v8h-6Z',GOLD)+path('M16 16v3', 'none',DARK)


def crown(color=GOLD):
    return path('M5 25 2 8l8 7 6-12 6 12 8-7-3 17Z','#62472a',color)+path('M5 22h22v6H5Z',DARK,color)+circle(16,18,2,color,color)


# Each silhouette has its own identity; related effects deliberately share a
# visual family. The palette is restrained to stay legible at 24-28 CSS pixels.
ICONS = {
    'gold': path('M3 19v7c0 5 16 5 16 0v-7', '#93622b', GOLD)+path('M3 22c0 4 16 4 16 0','none',GOLD)+
            '<ellipse cx="11" cy="19" rx="8" ry="3" fill="#e3af50" stroke="#f5d68a"/>'+circle(22,12,8,'#ba8436',GOLD)+path('M22 7v10m3-8c-6-3-7 4-1 4 3 0 2 4-4 3','none',GOLD),
    'chest': chest(),
    'rift': portal(BLUE),
    'battlefield': sword()+path('m3 4 6 1 19 23-3 2L6 9Z','#b09273')+path('m18 25 8-6','none',GOLD,stroke_width='2'),
    'cursed-orb': orb(RED)+path('m8 7 4 3m12-3-4 3','none',RED),
    'summon-portal': portal(GREEN)+path('m16 17 2 5-5-3h6l-5 3Z','none',GREEN),
    'chaos-pillar': path('M9 27 10 12 7 9l9-7 9 7-3 3 1 15Z','#514051')+path('m16 4 5 5-5 7-5-7Z','#725397',VIOLET)+path('M12 17h8l-4 4Zm-5 8h18v5H7Z',DARK)+path('M16 16v9','none',VIOLET),
    'chaos-tower': path('M7 29V13L5 10V5h5v4h4V3h4v6h4V5h5v5l-2 3v16Z','#46404c')+path('M13 29v-7a3 3 0 0 1 6 0v7M11 14h2m6 0h2M11 18h10','none',VIOLET)+path('M5 29h22','none',METAL),
    'shadow-realm': portal(VIOLET)+path('M19 9c-10 1-10 13 0 15-5-6-5-9 0-15Z','#765e9c',VIOLET),
    'dungeon-key': key(),
    'angelic-key': key('#e7d391')+path('M6 10 1 6v7l5 4m12-7 6-4v7l-5 4',DARK,'#e7d391'),
    'crystal-key': key(BLUE)+path('m12 6 3 4-3 4-3-4Z',BLUE,BLUE),
    'bifrost-key': key(GREEN)+path('m9 7 6 6m-6 0 6-6','none',BLUE),
    'ruby-key': key(RED)+path('m12 6 3 4-3 4-3-4Z',RED,RED),
    'relic': path('M12 3h8l3 8-4 8 5 10H8l5-10-4-8Z','#514132')+circle(16,12,5,'#34443e',GREEN)+path('m16 8 2 4-2 4-2-4Z',GREEN)+path('M11 25h10','none',GOLD),
    'rune': path('M9 3h15l5 19-7 7H6L3 19Z','#695948')+path('M12 8v16m0-13 8-3-3 8 5 6m-10-6 5-2','none','#ecceb0',stroke_width='2'),
    'gem': gem(BLUE),
    'boss-gem': gem(RED)+path('M9 3 11 7h10l2-4','none',GOLD),
    'orb': orb(),
    'scroll': path('M10 3h15v21H10c-5 0-6 5-2 5h15c6 0 7-6 2-6M10 3C3 1 3 9 8 9h3', '#b2946d')+path('M14 9h7m-7 5h6m-6 5h4','none',DARK),
    'shard': path('m17 2 7 8-4 20-9-7-3-12Z','#77649b',VIOLET)+path('m17 2-3 15 6 13m-6-13 10-7', 'none','#d7c6ed'),
    'battle-fragment': path('M4 5h10l3 5-4 5 6 7-5 8H4Z','#805348',RED)+path('M7 9h4v4H7m0 7 4 3-4 4','none',GOLD),
    'colosseum-fragment': path('M8 5 27 8v7l-5 3 4 8-19 3V19l5-3-4-5Z','#766249')+path('M14 11v8m5-7v6M10 24l12-2','none',GOLD),
    'experience': path('M16 3 26 8v13L16 29 6 21V8Z','#344b45',GREEN)+path('M11 20v-7m5 10V9m5 11v-7','none','#c1e1c7',stroke_width='2'),
    'magic-find': circle(13,13,9,'#453449',VIOLET)+path('m20 20 9 9','none',METAL,stroke_width='3')+star(13,13,GOLD),
    'boots': boot(),
    'damage': sword(),
    'attack-speed': sword()+path('M3 7h8M1 12h6M19 25l5-2-2 6','none',GOLD,stroke_width='1.5'),
    'cast-speed': path('M6 28 22 8l3 3L9 30Z','#745643')+star(22,9,VIOLET)+path('M3 8h7m-7 5h4m17 10 4-4','none',VIOLET),
    'life': heart(),
    'mana': drop(),
    'defense': shield(),
    'critical-damage': sword()+star(23,23,GOLD),
    'critical-chance': circle(16,16,10,DARK,RED)+circle(16,16,5,DARK,RED)+path('M16 1v9m0 12v9M1 16h9m12 0h9','none',GOLD)+circle(16,16,1.5,GOLD,GOLD),
    'spell-damage': path('m7 25 17-17','none',METAL,stroke_width='3')+star(20,12,VIOLET)+path('m5 19-2 8 9-2','none',VIOLET),
    'spell-chance': circle(16,16,11,DARK,VIOLET)+star(16,16,VIOLET)+path('M16 1v4m0 22v4M1 16h4m22 0h4','none',GOLD),
    'density': '<g transform="translate(0 1) scale(.72)">'+skull()+'</g><g transform="translate(10 9) scale(.72)">'+skull(RED)+'</g>',
    'rare': crown(),
    'ancient': skull('#e7c8b0')+path('M7 5 5 1l7 4m13 0 2-4-7 4','none',GOLD),
    'angelic': path('m16 8 3 8-3 12-3-12Z','#a49763',GOLD)+path('M13 12 2 5v9l5 4-3 1 7 5m8-12L30 5v9l-5 4 3 1-7 5', '#625845','#e9d79f')+circle(16,4,3,'none',GOLD),
    'map': path('m3 6 9-3 8 4 9-3v23l-9 3-8-4-9 3Z','#494d3b')+path('M12 3v23m8-19v23M6 19l4-8 5 9 7-9 4 4','none',GREEN)+circle(22,11,2,GOLD,GOLD),
    'relic-filter': path('M3 5h26L19 17v9l-6 3V17Z','#4e4633',GOLD)+path('m12 9 3 3 5-6','none',GREEN,stroke_width='2'),
    'pickup-orbs': circle(10,10,6,'#493650',VIOLET)+circle(22,10,6,'#3b4950',BLUE)+path('M6 21c0 9 20 9 20 0m-5 2 5-3 3 5','none',GOLD),
    'pet': circle(7,9,3,'#69533b')+circle(14,5,3,'#69533b')+circle(22,7,3,'#69533b')+circle(27,14,3,'#69533b')+path('M10 18c-8 10 1 15 6 9 7 5 16-4 7-11-4-6-8-4-13 2Z','#8d6b43',GOLD),
    'headhunter': skull(RED)+path('M3 28 29 3M24 3h5v5','none',GOLD,stroke_width='2'),
    'tyrant': crown(RED)+path('M11 18h10M16 16v6','none',GOLD),
    'beacon': path('M14 13h4v16h-4Z','#725039')+path('M16 2c4 7 8 8 5 13-6 6-15-2-9-7 0 4 5 3 4-6Z','#a65937',GOLD)+path('M4 7 7 9m18 0 3-2M2 17h5m18 0h5','none',GOLD),
    'light': circle(16,16,6,'#a07b40',GOLD)+path('M16 2v4m0 20v4M2 16h4m20 0h4M6 6l3 3m14 14 3 3M26 6l-3 3M9 23l-3 3','none',GOLD),
    'resistance': shield(GREEN)+path('m16 9-4 7 5-1-1 7 5-9h-6Z',GREEN,GREEN),
    'broken-armor': shield(RED,True),
    'darkness': path('M24 3C5-1-5 22 13 29c6 2 12-1 15-7C7 30 5 7 24 3Z','#59456d',VIOLET)+star(24,10,VIOLET),
    'skills': path('M3 7c5-3 9-2 13 1 4-3 8-4 13-1v21c-5-3-9-2-13 0-4-2-8-3-13 0Z','#4e3d55',VIOLET)+path('M16 8v20M7 12h5m-5 5h5m8-5h5m-5 5h5','none',METAL),
    'attributes': path('M16 3 29 13 24 28H8L3 13Z','#414333',GREEN)+path('M16 8v15m-7-9h14M10 24l6-7 6 7','none',GOLD),
    'recovery': heart(GREEN)+path('M3 6 2 14h7m20 12 1-8h-7','none',GOLD,stroke_width='2'),
    'clock': circle(16,17,11,DARK,METAL)+path('M12 2h8M16 6v11l6 3M24 5l3 3','none',GOLD),
    'boulder': path('m5 13 6-8 11 3 7 14-5 7H9l-6-8Z','#736354')+path('m11 5 3 11 15 6M5 13l9 3-5 13M3 4l3 4m17-6 4 5','none',METAL),
    'poison': path('M11 2h10v4l-2 2v7c13 13 8 15-3 15S0 28 13 15V8l-2-2Z','#374333',GREEN)+path('M8 23h16','none',GREEN)+circle(14,23,2,GREEN,GREEN)+circle(19,26,1,GREEN,GREEN),
    'fire': path('M17 2c0 10 10 10 10 19C27 34 3 33 5 20c0-5 4-8 6-12-1 12 9 7 6-6Z','#804934',GOLD)+path('M17 16c0 6-6 6-5 10 1 5 10 1 7-4Z',GOLD,GOLD),
    'bleed': drop(RED)+path('M23 5v8m-4-4h8','none',RED),
    'slow': boot()+path('M3 5h6M2 10h6M1 15h5','none',BLUE),
    'monster-life': skull(RED)+circle(24,25,6,'#633b38',RED)+path('M24 22v6m-3-3h6','none','#f3d0bc'),
    'goblin': path('M9 9 1 6l4 11 4 1c0 14 14 14 14 0l4-1 4-11-8 3C22 1 10 1 9 9Z','#485c3c',GREEN)+path('m10 14 4 2m8-2-4 2m-5 8h6M16 16l-2 4h4','none',GOLD),
    'settings': path('m12 3 1 4-4 2-4-1-3 7 4 2v4l-2 3 6 5 3-3h5l3 3 6-5-2-3v-4l4-2-3-7-4 1-4-2-1-4Z','#4a3d31')+circle(16,17,5,DARK,GOLD),
    'folder': path('M3 7h10l3 3h13v18H3Z','#6d5133')+path('M3 15h27l-4 13H3Z','#967146',GOLD),
    'save': path('M5 3h20l4 4v22H3V3Z','#63503c')+path('M9 3h13v9H9ZM9 20h14v9H9Z',DARK,GOLD)+path('M18 5v5','none',GOLD),
    'install': path('M5 20v9h22v-9M16 2v21m-6-6 6 6 6-6','none',GREEN,stroke_width='2'),
    'remove': path('M7 8h18l-2 21H9ZM4 8h24M12 4h8M13 13v10m6-10v10',DARK,RED),
    'play': path('m9 4 20 12-20 12Z','#775331',GOLD),
    'auto-apply': path('M26 12A11 11 0 0 0 6 8L3 5v9h9l-3-3M6 20a11 11 0 0 0 20 4l3 3v-9h-9l3 3','none',GOLD,stroke_width='1.8'),
}

CONTROL_ICONS = {
    'drops': {'gold':'gold', 'mining_ore':'gem'},
    'spawners': dict(zip(['rift','battlefield','cursedorb','summonportal','chaospillars','chaostower','shadowrealm'],
                         ['rift','battlefield','cursed-orb','summon-portal','chaos-pillar','chaos-tower','shadow-realm'])),
    'keys': dict(zip(['dungeon','angelic','chaos','bifrost','relic','rune','stone','bossgem','orb','scrollofra','dimshard','battlefrag','colosfrag','ruby'],
                    ['dungeon-key','angelic-key','crystal-key','bifrost-key','relic','rune','gem','boss-gem','orb','scroll','shard','battle-fragment','colosseum-fragment','ruby-key'])),
    'stats': {'exp':'experience','magicfind':'magic-find','movespeed':'boots'},
    'percent_stats': dict(zip(['damage','attackspeed','castrate','lifereplenish','manareplenish','defense','critdamage','critchance','spellcritdamage','spellcritchance'],
                             ['damage','attack-speed','cast-speed','life','mana','defense','critical-damage','critical-chance','spell-damage','spell-chance'])),
}
STATIC_ICONS = {
    'den':'density','enemyspeed':'boots','enemyspeed_ct':'chaos-tower',
    'angelic_items':'angelic','rarity_rare':'rare','rarity_ancient':'ancient',
    'map_reveal':'map','map_reveal_packs':'density','mod_filter_max_relics':'relic-filter',
    'mod_orb_pickup_radius':'pickup-orbs','mod_pet_quest_pickup':'pet',
    'headhunter':'headhunter','tyrant':'tyrant','beacon':'beacon',
}
SECTION_ICONS = {
    'setupCard':'settings','densityCard':'density','speedCard':'boots',
    'spawnsCard':'rift','dropsCard':'chest','angelicCard':'angelic',
    'rarityCard':'rare','satanicMods':'cursed-orb','gameplayCard':'settings','itemsCard':'relic',
}
ACTION_ICONS = {'exebrowse':'folder','exesave':'save','installmod':'install',
                'removeplugin':'remove','launchgame':'play','applyall':'auto-apply'}
SATANIC_ICONS = {
    'buff': dict(enumerate(['chest','chest','rune','gold','rare','angelic','boots','attack-speed','cast-speed','damage','spell-damage','relic','goblin','magic-find','magic-find','magic-find','experience','experience','experience','recovery','density','critical-damage','ancient','ancient','ancient'],1)),
    'debuff': dict(enumerate(['light','resistance','broken-armor','life','mana','darkness','skills','attributes','recovery','bleed','mana','clock','clock','boulder','slow','critical-damage','monster-life','monster-life','monster-life','bleed','poison','fire','attack-speed','broken-armor','bleed','slow'],1)),
}


def _group(body):
    return f'<g stroke-width="1.35" stroke-linejoin="round" stroke-linecap="round">{body}</g>'


def svg(name):
    """Standalone icon for reuse in another frontend, including its palette."""
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32" fill="none">{_group(ICONS[name])}</svg>'


ICON_SPRITE = '<svg xmlns="http://www.w3.org/2000/svg" class="icon-definitions" width="0" height="0" aria-hidden="true" focusable="false"><defs>' + ''.join(
    f'<symbol id="fp-icon-{name}" viewBox="0 0 32 32" fill="none">{_group(body)}</symbol>'
    for name, body in ICONS.items()) + '</defs></svg>'
ICON_MAP_JS = 'const PANEL_ICON_MAP=' + json.dumps(dict(controls=CONTROL_ICONS,static=STATIC_ICONS,sections=SECTION_ICONS,actions=ACTION_ICONS,satanic=SATANIC_ICONS),separators=(',',':')) + ';\n'


def export_pack(directory):
    directory=Path(directory)
    directory.mkdir(parents=True,exist_ok=True)
    for name in ICONS:
        (directory/f'{name}.svg').write_text(svg(name),encoding='utf-8')
    cards=''.join(f'<figure><img src="{name}.svg" alt=""><figcaption>{html.escape(name)}</figcaption></figure>' for name in ICONS)
    gallery='<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ForgePact Icon Pack</title><style>body{margin:40px;background:#100d0b;color:#eee2d2;font:14px Segoe UI,sans-serif}h1{font-size:28px}p{color:#b7a996}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(135px,1fr));gap:12px}figure{margin:0;padding:20px 12px;background:#211b16;border:1px solid #43362a;border-radius:10px;text-align:center}img{width:48px;height:48px}figcaption{margin-top:12px;color:#e9c7a4;font-size:12px}</style><h1>ForgePact · Icon Pack</h1><p>Original SVG artwork · Falor · 32 × 32 viewBox · AGPL-3.0</p><main>'+cards+'</main></html>'
    (directory/'index.html').write_text(gallery,encoding='utf-8')
    (directory/'README.txt').write_text('ForgePact icon pack\nOriginal UI artwork created for Falor\nLicense: AGPL-3.0, same as ForgePact (see the project LICENSE).\nNo game-extracted artwork, runtime libraries or remote requests.\nEvery SVG is self-contained. Recommended UI size: 24-28 px.\nKeep the visible setting name beside the icon.\nSource: ForgePact/src/panel_icons.py\n',encoding='utf-8')
    return len(ICONS)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description='Export the ForgePact SVG icon pack.')
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    print(f'Exported {export_pack(args.output)} icons to {args.output.resolve()}')
