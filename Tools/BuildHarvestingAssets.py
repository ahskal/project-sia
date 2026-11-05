"""Update item sources, import the native row schema, and create harvest assets."""

import runpy
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
runpy.run_path(str(TOOLS / "UpdateHarvestItems.py"), run_name="__main__")
runpy.run_path(str(TOOLS / "ImportItems.py"))
runpy.run_path(str(TOOLS / "CreateHarvestProfiles.py"))
