EverythingRandomizer can randomize many things:

- Loot table
- potion/poison/food effect
- enchantment effect
- ingredient effect
- ...

Modify the configuration via `EverythingRandomizer_config.lua`, which includes
explanatory comments inside the file.

### Generating EverythingRandomizer_protection.lua

This prevents quest-related data from being randomized. Download `protectgen`
from Releases, run the program, and follow the instructions.

### FAQ

> Can I install/uninstall mid-game?

It's hard to say. The short answer is sort of yes and no. From
EverythingRandomizer's perspective, it is stateless and independent of saves, so
technically you can. However, it still modifies Form data, and many scripts
might get polluted by unexpected data. So, you can't. It's up to you.

> Compatibility?

Similar to the previous question, it's hard to say. If
`EverythingRandomizer_protection.lua` is generated correctly, it should be
_okay_. But the main issue remains the scripts. When playing with my own
modlist, it was stable most of the time.
