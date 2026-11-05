"""Add the harvesting materials and tools to the source item table idempotently."""

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ITEMS_PATH = ROOT / "Content" / "Data" / "Source" / "Items.json"
STONE_MESH = (
    "/Game/Sia/Harvesting/Rocks/Fragments/SM_RockGroup01/"
    "SM_RockGroup01_Piece04.SM_RockGroup01_Piece04"
)


def material(name, display_name, description, icon, mesh, weight, sell_value):
    return {
        "Name": name,
        "ItemType": "Material",
        "ItemQuality": "Common",
        "ItemStatistics": {
            "ArmorRating": 0,
            "DamageValue": 0,
            "RestorationAmount": 0,
            "SellValue": sell_value,
        },
        "TextData": {
            "DisplayName": display_name,
            "Description": description,
            "UsageText": "제작 재료",
        },
        "NumericData": {"Weight": weight, "MaxStackSize": 50},
        "AssetData": {"Icon": icon, "Mesh": mesh},
        "EquipmentSlot": "None",
    }


def tool(name, display_name, tool_type, mesh, icon=""):
    return {
        "Name": name,
        "ItemType": "Weapon",
        "ItemQuality": "Common",
        "EquipmentSlot": "Weapon",
        "HarvestToolType": tool_type,
        "ItemStatistics": {
            "ArmorRating": 0,
            "DamageValue": 10,
            "RestorationAmount": 0,
            "SellValue": 20,
        },
        "TextData": {
            "DisplayName": display_name,
            "Description": f"자원 채집에 사용하는 {display_name}입니다.",
            "UsageText": "무기 장비칸에 장착",
        },
        "NumericData": {"Weight": 2.0, "MaxStackSize": 1},
        "AssetData": {"Icon": icon, "Mesh": mesh},
    }


def main():
    rows = json.loads(ITEMS_PATH.read_text(encoding="utf-8-sig"))
    by_name = {row["Name"]: row for row in rows}
    if len(by_name) != len(rows):
        raise ValueError("Items.json contains duplicate item row names.")

    stone = by_name.get("Stone")
    if not stone:
        raise ValueError("The existing Stone row is missing; refusing to invent a replacement.")
    stone["AssetData"]["Mesh"] = STONE_MESH

    default_grip = {
        "Rotation": {"X": 0, "Y": 0, "Z": 0, "W": 1},
        "Translation": {"X": 0, "Y": 0, "Z": 0},
        "Scale3D": {"X": 0.55, "Y": 0.55, "Z": 0.55},
    }
    for row in rows:
        row.setdefault("HarvestToolType", "None")
        row["ToolGripTransform"] = {
            field: values.copy() for field, values in default_grip.items()
        }

    wood_mesh = "/Game/Mega_Survival_Tools/Models/SM_Log.SM_Log"
    additions = [
        material("Log", "통나무", "벌목한 나무에서 얻는 통나무입니다.",
                 "/Game/50_Stylized_Icons/Textures/T_Firewood.T_Firewood",
                 wood_mesh, 1.5, 3),
        tool("Axe", "도끼", "Axe",
             "/Game/Sia/Harvesting/Tools/Axes/SM_Axe.SM_Axe",
             "/Game/50_Stylized_Icons/Textures/T_Falmer_Axe_74.T_Falmer_Axe_74"),
        tool("Hammer", "망치", "Hammer",
             "/Game/Sia/Harvesting/Tools/Hammers/SM_Hammer.SM_Hammer",
             "/Game/50_Stylized_Icons/Textures/T_hammer_9.T_hammer_9"),
        tool("Pickaxe", "곡괭이", "Pickaxe",
             "/Game/Sia/Harvesting/Tools/Pickaxes/SM_Pickaxe.SM_Pickaxe",
             "/Game/50_Stylized_Icons/Textures/T_Whetstone.T_Whetstone"),
        tool("Hoe", "괭이", "Hoe",
             "/Game/Sia/Harvesting/Tools/Hoes/SM_Hoe.SM_Hoe"),
        tool("Shovel", "삽", "Shovel",
             "/Game/Sia/Harvesting/Tools/Shovels/SM_Shovel.SM_Shovel"),
    ]
    for row in additions:
        existing = by_name.get(row["Name"])
        if existing is None:
            rows.append(row)
            by_name[row["Name"]] = row
        else:
            fields_to_match = [
                ("ItemType", row["ItemType"]),
                ("EquipmentSlot", row["EquipmentSlot"]),
                ("TextData.DisplayName", row["TextData"]["DisplayName"]),
                ("AssetData.Mesh", row["AssetData"]["Mesh"]),
            ]
            if "HarvestToolType" in row:
                fields_to_match.append(("HarvestToolType", row["HarvestToolType"]))
            for dotted_field, expected in fields_to_match:
                value = existing
                for field in dotted_field.split("."):
                    value = value.get(field) if isinstance(value, dict) else None
                if value != expected:
                    raise ValueError(f"Existing {row['Name']} row differs at {dotted_field}.")

    ITEMS_PATH.write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Updated {ITEMS_PATH}: {len(rows)} unique item rows; Stone uses one extracted rock piece.")


if __name__ == "__main__":
    main()
