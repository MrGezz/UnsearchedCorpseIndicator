# Features

- Shows an icon above the corpses of dead NPCs that you have not searched.
- Can be configured to show for all corpses, or only those killed by you or a follower. By default, the unsearched icon shows for all  corpses.
- Can be set to show immediately upon death, or set to a custom delay.
- Icon automatically disappears once you search the body, even if you leave items behind.
- Icon display can be toggled on/off via a configurable hotkey (L by default).

# Requirements

- Status Indicator Framework﻿ (SIF) is required

# Installation

- Install with your mod manager of choice.
- Can install and uninstall anytime.

# Configuration

Navigate to `SKSE\Plugins\UnsearchedCorpsesIndicator.ini` to adjust the following settings:

- `fDelay` - can be set to any positve number to delay how long after death the icon appears above the corpse.
- `bAllCorpses` - Show icons on all dead NPCs, not just ones killed by you or followers (default: 1). 0 = player/follower kills only, 1 = all corpses
- `iToggleKey` - DirectX scan code for toggling icons on/off (default: 38 = L)

If you navigate to `SKSE\Plugins\SIF\UnsearchedCorpsesIndicator.json`, you can also adjust the following:

- `"fadeMaxDistance": 1500` - The distance at which the icon fully disappears. Increase this value if you want icons to remain visible from farther away.
- `"fadeStartDistance": 1000` - The distance at which the icon begins fading out. Icons are fully visible before this distance, then gradually fade until they reach fadeMaxDistance.
- `"maxInstances": 20` - The maximum number of corpse icons that can be displayed at the same time. Increase this if you want more icons visible at once. Decrease it if you want to reduce screen clutter.

# Credits

- JerryYOJ for Status Indicator Framework - please be sure to endorse over there.
- SKSE team.
- CommonLibSSE contributors.