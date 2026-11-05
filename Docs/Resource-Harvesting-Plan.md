# Sia resource harvesting handoff

이 문서는 나무·바위 채집과 도구 공격 기능의 현재 구현, 조정 위치, 검증 결과를 기록합니다. 다음 작업 지시를 내리기 전에 이 문서를 참고할 수 있습니다.

## 현재 상태

- `BP_TreeResource`를 공격하면 나무가 공격 방향으로 넘어지고 `Log`가 떨어집니다. 사과는 프로필 확률에 따라 추가로 떨어집니다.
- `BP_RockResource`는 공격 위치와 관계없이 바깥 조각부터 분리됩니다. 마지막에 `Stone`을 떨어뜨립니다.
- 자원 크기는 인스턴스의 `Resource Size`로 정하며, 드롭 수량은 프로필의 `Size Yield Exponent`로 배율을 적용합니다.
- 도끼, 망치, 곡괭이, 괭이, 삽은 기존 `Weapon` 장비 슬롯에 들어갑니다. 장착 도구는 캐릭터의 `hand_r`에 표시됩니다.
- 도끼는 나무에, 곡괭이는 바위에 기본 피해의 1.5배를 줍니다.
- 공격은 `MM_Attack_01`과 `MM_Attack_02`를 번갈아 재생합니다. 공격 중 입력 하나를 예약할 수 있습니다. 양손의 충돌 상자는 같은 공격에서 같은 대상을 한 번만 처리합니다.

## 에디터에서 조정할 위치

| 에셋 | 조정 항목 | 기본값 |
| --- | --- | --- |
| `/Game/Sia/Harvesting/Trees/Profiles/DA_TreeHarvestProfile` | 최대 체력, 도끼 배율, 통나무 수량, 사과 확률과 수량, 넘어지는 시간과 각도 | 체력 100, 도끼 1.5배, 통나무 2–4개, 사과 18% 확률로 1–2개 |
| `/Game/Sia/Harvesting/Rocks/Profiles/DA_RockHarvestProfile` | 최대 체력, 곡괭이 배율, 돌 수량, 타격당 분리 조각 수 | 체력 80, 곡괭이 1.5배, 돌 2–4개, 타격당 조각 1개 |
| 레벨 인스턴스 `BP_TreeResource` 또는 `BP_RockResource` | `Resource Size` | 1.0 |
| `/Game/Data/DT_Items` 및 `Content/Data/Source/Items.json` | 도구 종류, 장비 슬롯, 픽업 메시, 아이템 정의 | 24개 행 |

드롭 확률과 수량은 `FHarvestDropRule`로 관리합니다. 크기 배율은 프로필마다 조정할 수 있습니다. 중복된 아이템 규칙은 하나의 픽업으로 합칩니다.

## 사용 에셋과 정리 위치

| 용도 | 프로젝트 에셋 경로 |
| --- | --- |
| 나무 액터 | `/Game/Sia/Harvesting/Trees/BP_TreeResource` |
| 나무 프로필 | `/Game/Sia/Harvesting/Trees/Profiles/DA_TreeHarvestProfile` |
| 나무 메시 | `/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree01/SM_green-tree01` |
| 통나무 픽업과 작은 충격 조각 | `/Game/Mega_Survival_Tools/Models/SM_Log` |
| 바위 액터 | `/Game/Sia/Harvesting/Rocks/BP_RockResource` |
| 바위 프로필 | `/Game/Sia/Harvesting/Rocks/Profiles/DA_RockHarvestProfile` |
| 분리된 바깥 조각 | `/Game/Sia/Harvesting/Rocks/Fragments/SM_RockGroup01/SM_RockGroup01_Piece01`부터 `Piece04`까지 |
| 돌 픽업 메시 | `SM_RockGroup01_Piece04` |
| 도구 메시 | `/Game/Sia/Harvesting/Tools/Axes`, `Hammers`, `Pickaxes`, `Hoes`, `Shovels` |
| 도구 원본 묶음 | `/Game/Fab/Survival_Game_Tool_Pack/survival_game_tool_pack_free_agustin_honnun/StaticMeshes/survival_game_tool_pack_free_agustin_honnun` |

사용 에셋과 원본 위치는 `Tools/HarvestAssetManifest.json`, `Tools/HarvestRockGroupManifest.json`, `Tools/HarvestToolExtractionManifest.json`에도 기록했습니다. 추출 스크립트는 원본 묶음을 바꾸지 않고 역할별 메시를 `/Game/Sia/Harvesting` 아래에 만듭니다.

## 구현 위치

- `Source/Sia/Data/HarvestDataTypes.h`와 `.cpp`: 도구·드롭 데이터, 프로필 유효성 검사, 피해와 드롭 수량 계산.
- `Source/Sia/World/HarvestableResource.*`: 공통 체력, 중복 보상 방지, 아이템 픽업 생성.
- `Source/Sia/World/TreeResource.*`: 나무 쓰러짐과 작은 통나무 물리 조각.
- `Source/Sia/World/RockResource.*`와 `HarvestDebris.*`: 바깥부터 조각을 분리하고 짧은 물리 효과를 적용.
- `Source/Sia/BaseCharacter.*`: 양손 공격 판정, 공격 입력 예약, 장비 도구 표시와 자원 타격.
- `Source/Sia/CharacterAnimationComponent.*`: `MM_Attack_01`과 `MM_Attack_02` 재생.
- `Source/Sia/Tests/HarvestingTests.cpp`: 자원·장비·전투 동작 자동화 검사.

## 확인한 항목

- Unreal Engine 5.8 `SiaEditor Win64 Development` 빌드가 통과했습니다.
- `UnrealEditor-Cmd`에서 `Automation RunTests Sia`를 실행했습니다. 16개 Sia 자동화 검사가 모두 통과했고 종료 코드는 0이었습니다.
- 나무의 실제 채집 경로에서 통나무 픽업 메시 경계가 바닥 `Z=0`에 놓이고, 나무가 넘어지며, 한 번만 보상하는 것을 확인했습니다.
- 서로 다른 위치를 친 두 바위가 같은 첫 번째 조각을 내고, 바위가 돌 픽업을 만드는 것을 확인했습니다.
- 오른손 판정을 치운 상태에서 왼손 판정만으로 나무에 피해를 주고, 장착 도끼 배율을 적용하는 것을 확인했습니다.
- 공격 입력 예약이 `MM_Attack_01` 다음 `MM_Attack_02`를 재생하는 것을 확인했습니다.

## 제한과 다음 확인

- 나무 본체는 실제 Geometry Collection 파쇄가 아니라 회전해 넘어집니다. 맞은 지점에는 작은 `SM_Log` 물리 조각을 만듭니다. 현재 프로젝트에서 사용할 Geometry Collection 나무 파쇄 에셋을 확인하지 못했습니다.
- 자동화 검사는 에디터 명령행의 테스트 월드에서 실행했습니다. PIE 화면에서 도구가 손에 맞게 보이는지, 애니메이션과 파티클이 레벨에서 자연스러운지는 아직 확인하지 않았습니다.
- 테스트에 쓰인 바닥 검사는 메시 경계의 최저점으로 높이를 비교합니다. 예전 채집 검사는 테스트 바닥의 생성 순서가 달라 10cm 오차를 냈습니다. 기존 인벤토리 바닥 배치 방식과 같은 순서로 고친 뒤 채집 로그와 인벤토리 드롭 검사가 모두 통과했습니다.

레벨에서 확인하려면 콘텐츠 브라우저의 `BP_TreeResource`와 `BP_RockResource`를 배치하고 플레이를 시작하세요. 사과 드롭은 18% 확률이므로 여러 나무를 공격해야 확인할 수 있습니다. 도구 메시의 손 위치나 크기를 조정하려면 `DT_Items`의 `ToolGripTransform`을 수정하세요.
