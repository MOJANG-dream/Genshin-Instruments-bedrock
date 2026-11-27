r"""Generates the resource and behaviour packs for the Genshin Instruments port.

Usage:  python tools/build_packs.py [java-assets-dir]
Default source: E:\Github\Genshin-Instruments-src\src\main\resources\assets\genshinstrument
Target: data/packs/RP and data/packs/BP

Bedrock scans a *parent* directory for pack folders, so the packs themselves
live one level down: data/packs/RP/genshin_instruments/manifest.json. The
runtime registers data/packs/RP and data/packs/BP as custom pack sources.
"""

from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

DEFAULT_SRC = Path(r"E:\Github\Genshin-Instruments-src\src\main\resources\assets\genshinstrument")
# The argument may be that assets directory itself or a checkout root of the
# Java mod (CI clones the upstream repository).
SUFFIX = Path("src/main/resources/assets/genshinstrument")

if len(sys.argv) > 1:
    arg = Path(sys.argv[1]).resolve()
    SRC = arg if (arg / "sounds").is_dir() else arg / SUFFIX
else:
    SRC = DEFAULT_SRC

DST = Path(__file__).resolve().parent.parent / "data" / "packs"

if not (SRC / "sounds").is_dir():
    raise SystemExit(f"sound assets not found under: {SRC}")

RP = DST / "RP" / "genshin_instruments"
BP = DST / "BP" / "genshin_instruments"

RP_UUID = "7d3f1a2b-9c4e-4b7a-8f1d-2e6c5a9b0d41"
RP_MODULE_UUID = "b6c1d2e3-4f5a-4b7c-8d9e-0f1a2b3c4d5e"
BP_UUID = "2b6c8e4f-1a3d-4f5b-9c7e-6d2a0b8f4c53"
BP_MODULE_UUID = "c7d2e3f4-5a6b-4c8d-9e0f-1a2b3c4d5e6f"
PACK_VERSION = [0, 1, 0]

# Java sound directory -> (sound group used in definition keys, sample count)
SOUND_GROUPS = {
    "windsong_lyre": ("windsong_lyre", 21),
    "floral_zither_new": ("floral_zither_new", 21),
    "floral_zither_old": ("floral_zither_old", 21),
    "vintage_lyre": ("vintage_lyre", 21),
    "ukulele": ("ukulele", 21),
    "djem_djem_drum": ("djem_djem_drum", 8),
    "glorious_drum": ("glorious_drum", 2),
    "nightwind_horn/attack": ("nightwind_horn_attack", 14),
    "nightwind_horn/hold": ("nightwind_horn_hold", 14),
}

# instrument id (item), display name, rp texture key suffix, icon file in the Java assets
ITEMS = [
    ("windsong_lyre", "Windsong Lyre", "windsong_lyre.png"),
    ("floral_zither", "Floral Zither", "floral_zither.png"),
    ("vintage_lyre", "Vintage Lyre", "vintage_lyre.png"),
    ("glorious_drum", "Arataki's Great and Glorious Drum", "glorious_drum.png"),
    ("nightwind_horn", "Nightwind Horn", "nightwind_horn.png"),
    ("ukulele", "Ukulele", "ukulele.png"),
    ("djem_djem_drum", "Djem Djem Drum", "djem_djem_drum.png"),
]


def write_json(path: Path, data) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as fh:
        json.dump(data, fh, indent=2, ensure_ascii=False)
        fh.write("\n")


def copy_sounds() -> dict:
    """Copies every sample and returns the sound definitions."""
    definitions = {}
    for java_dir, (group, _count) in SOUND_GROUPS.items():
        source = SRC / "sounds" / java_dir
        if not source.is_dir():
            raise SystemExit(f"missing sound folder: {source}")
        target = RP / "sounds" / "genshin" / group
        target.mkdir(parents=True, exist_ok=True)

        for ogg in sorted(source.glob("*.ogg")):
            stem = ogg.stem
            name = stem.replace(".stereo", "_stereo")
            out = target / f"{name}.ogg"
            shutil.copyfile(ogg, out)
            definitions[f"genshin.{group}.{name}"] = {
                "category": "player",
                "sounds": [
                    {
                        "name": f"genshin/{group}/{name}",
                        "volume": 1.0,
                        "pitch": 1.0,
                        "stream": False,
                        "load_on_low_memory": True,
                    }
                ],
            }
    return definitions


def write_sound_definitions(definitions: dict) -> None:
    write_json(
        RP / "sounds" / "sound_definitions.json",
        {"format_version": "1.14.0", "sound_definitions": definitions},
    )


def copy_item_icons() -> dict:
    textures = {}
    target = RP / "textures" / "items"
    target.mkdir(parents=True, exist_ok=True)

    for item_id, _name, icon in ITEMS:
        source = SRC / "textures" / "item" / icon
        if not source.is_file():
            raise SystemExit(f"missing icon: {source}")
        shutil.copyfile(source, target / f"{item_id}.png")
        textures[f"genshin_{item_id}"] = {"textures": f"textures/items/{item_id}"}

    write_json(
        RP / "textures" / "item_texture.json",
        {
            "resource_pack_name": "genshin_instruments",
            "texture_name": "atlas.items",
            "texture_data": textures,
        },
    )
    return textures


def write_manifests() -> None:
    write_json(
        RP / "manifest.json",
        {
            "format_version": 2,
            "header": {
                "name": "Genshin Instruments Resources",
                "description": "Instrument samples and item icons for the Genshin Instruments mod.",
                "uuid": RP_UUID,
                "version": PACK_VERSION,
                "min_engine_version": [1, 21, 0],
            },
            "modules": [
                {
                    "type": "resources",
                    "uuid": RP_MODULE_UUID,
                    "version": PACK_VERSION,
                }
            ],
        },
    )
    write_json(
        BP / "manifest.json",
        {
            "format_version": 2,
            "header": {
                "name": "Genshin Instruments Behaviour",
                "description": "Instrument items for the Genshin Instruments mod.",
                "uuid": BP_UUID,
                "version": PACK_VERSION,
                "min_engine_version": [1, 21, 0],
            },
            "modules": [
                {
                    "type": "data",
                    "uuid": BP_MODULE_UUID,
                    "version": PACK_VERSION,
                }
            ],
        },
    )


def write_items() -> None:
    for item_id, name, _icon in ITEMS:
        write_json(
            BP / "items" / f"{item_id}.json",
            {
                "format_version": "1.21.10",
                "minecraft:item": {
                    "description": {
                        "identifier": f"genshin:{item_id}",
                        "menu_category": {"category": "items"},
                    },
                    "components": {
                        # "texture" is the 1.21.10 form; newer engines accept it too.
                        "minecraft:icon": {"texture": f"genshin_{item_id}"},
                        "minecraft:display_name": {"value": name},
                        "minecraft:max_stack_size": 1,
                        "minecraft:hand_equipped": True,
                        "minecraft:use_modifiers": {"use_duration": 3600.0, "movement_modifier": 1.0},
                        "minecraft:should_despawn": False,
                        "minecraft:foil": False,
                        "minecraft:can_destroy_in_creative": False,
                    },
                },
            },
        )


def main() -> None:
    if not SRC.is_dir():
        raise SystemExit(f"java assets not found: {SRC}")

    definitions = copy_sounds()
    write_sound_definitions(definitions)
    copy_item_icons()
    write_manifests()
    write_items()

    ogg_count = len(list(RP.rglob("*.ogg")))
    print(f"packs written to {DST}")
    print(f"  sound definitions: {len(definitions)}")
    print(f"  ogg files:         {ogg_count}")
    print(f"  items:             {len(ITEMS)}")


if __name__ == "__main__":
    main()
