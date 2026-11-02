"""언리얼 에디터의 Python 스크립트 실행 메뉴에서 실행한다."""
import json
from pathlib import Path
import unreal

source = Path(unreal.Paths.project_content_dir()) / "Data/Source/Items.json"
rows = json.loads(source.read_text(encoding="utf-8-sig"))
names = [row["Name"] for row in rows]
if len(set(names)) != len(names):
    raise ValueError("아이템 행 이름이 중복됩니다.")
for row in rows:
    numeric = row["NumericData"]
    if not row["TextData"]["DisplayName"] or numeric["Weight"] < 0 or numeric["MaxStackSize"] < 1:
        raise ValueError(f"아이템 데이터 오류: {row['Name']}")

table = unreal.load_asset("/Game/Data/DT_Items")
if not isinstance(table, unreal.DataTable):
    raise RuntimeError("/Game/Data/DT_Items 데이터 테이블이 필요합니다.")
if not table.fill_from_json_string(json.dumps(rows, ensure_ascii=False), table.get_row_struct()):
    raise RuntimeError("아이템 JSON 반영에 실패했습니다. 출력 로그를 확인하세요.")
unreal.EditorAssetLibrary.save_loaded_asset(table, only_if_is_dirty=False)
unreal.log(f"DT_Items: {len(rows)}개 행 반영 완료")
