EverythingRandomizer can randomize many things:

- Loot table
- potion/poison/food effect
- enchantment effect
- ingredient effect
- ...

Modify the configuration via `EverythingRandomizer_config.lua`, which includes
explanatory comments inside the file.

### How to start

1. Install the mod and it's dependencies normally.
2. (optional) Open file:
   `ModFolder/SKSE/Plugins/LuaPatcher/Scripts/EverythingRandomizer_config.lua`.
   Change "seed" to ANY integer.
3. (optional but recommanded) Generating EverythingRandomizer_protection.lua.
   See below.

### Generating EverythingRandomizer_protection.lua

This prevents quest-related data from being randomized. Download `protectgen`
from Releases, run the program, and follow the instructions.

I suggest you set the output path to an empty mod, similar to how BodySlide
works. Remember that the output mod should override EverythingRandomizer.

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

> Work for items added by other mods?

Yes. You might also want [GearInjection](../GearInjection/README.md) to add
items from other mods into the loot tables.
