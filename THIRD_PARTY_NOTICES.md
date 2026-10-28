# Third-party notices

This plugin builds on the following work.

## Borderlands Loot Randomizer, by mopioid

https://github.com/mopioid/Borderlands-Loot-Randomizer

The RANDO tab is a PS4 port of Loot Randomizer for the PC version of Borderlands 2:
- `source/bl2_editor/personal/rando_data.h` is generated from Loot Randomizer's item, location and option data by `gen_rando.py`;
- the seed logic in `pyrand.h` and `randomizer.c` is rebuilt from Loot Randomizer's, so that seeds match the PC mod.

Loot Randomizer has no license file, so all rights remain with mopioid. It's used here with mopioid's permission. If you use or adapt the randomizer parts, please ask mopioid as well.

## GoldHEN Plugins SDK (MIT)

https://github.com/GoldHEN/GoldHEN_Plugins_SDK, Copyright (c) 2022 GoldHEN. The SDK's sources (Detour, Patcher, syscalls and so on) are built into the plugin. Full text: [licenses/GoldHEN_Plugins_SDK-MIT.txt](licenses/GoldHEN_Plugins_SDK-MIT.txt).

## GoldHEN Plugins Repository (GPL-3.0)

https://github.com/GoldHEN/GoldHEN_Plugins_Repository. Its `common/plugin_common.c` (notifications, based on OSM's PS4-Notify: https://github.com/OSM-Made/PS4-Notify) is built into the plugin. Full text: [licenses/GoldHEN_Plugins_Repository-GPL-3.0.txt](licenses/GoldHEN_Plugins_Repository-GPL-3.0.txt).

## Mersenne Twister (MT19937)

`pyrand.h` reimplements the MT19937 generator and its `init_by_array` seeding, as used by CPython's `random` module.

```
Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  3. The names of its contributors may not be used to endorse or promote
     products derived from this software without specific prior written
     permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

## CPython `random` module

The seeding from bytes (SHA-512), `randbelow`, `shuffle` and `sample` in `pyrand.h` reproduce the algorithms in CPython's `Lib/random.py` (Python Software Foundation License) so that seeds give the same results as the PC mod.

## Item codes

The codes in `bl2_codes.txt` come from lootlemon.com, gamertweak.com, gameplay.tips and windowsreport.com, in the item code format of Gibbed's Borderlands 2 save editor.
