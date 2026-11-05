# Sia resource harvesting handoff

이 문서는 나무·바위 채집과 도구 공격 기능의 현재 구현, 조정 위치, 검증 결과를 기록합니다. 다음 작업 지시를 내리기 전에 이 문서를 참고할 수 있습니다.

## 현재 상태

- `BP_TreeResource`를 공격하면 나무가 공격 방향으로 넘어지고 `Log`가 떨어집니다. 사과는 프로필 확률에 따라 추가로 떨어집니다.
- 나무는 `GreenTrees/tree01`부터 `tree05`까지의 메시 중 하나를 시작할 때 고릅니다. 충돌 상자는 몸통 크기인 반지름 35cm, 높이 320cm이며 Pawn 채널을 막아 플레이어가 통과하지 못합니다.
- `BP_RockResource`는 공격 위치와 관계없이 바깥 조각부터 분리합니다. 체력이 0이 되면 선택한 바위에 대응하는 Chaos Geometry Collection으로 부서지고 `Stone`을 떨어뜨린 뒤 3초 후 사라집니다.
- 바위 본체는 `SM_RockPack_RockGroups_01_A`, `SM_RockPack_RockGroups_02_B`, `SM_RockPack_Rock_07` 중 하나를 고릅니다. 각 메시와 같은 순서의 파쇄 Geometry Collection을 사용합니다. 일반 타격 때 나오는 작은 돌 조각과 돌 픽업은 `SM_RockGroup01`에서 분리한 메시를 씁니다.
- 바위 충돌 상자는 메시의 X/Y 경계에 맞추고 Z만 프로필의 `Collision Vertical Scale`(기본 1.4배)로 늘립니다. 체력이 줄 때마다 메시와 충돌 상자를 함께 줄이며, 파괴 시 자원 충돌은 끄고 Geometry Collection과 작은 파편은 Pawn 채널을 무시합니다.
- 자원 크기는 인스턴스의 `Resource Size`로 정하며, 드롭 수량은 프로필의 `Size Yield Exponent`로 배율을 적용합니다.
- 도끼, 망치, 곡괭이, 괭이, 삽은 기존 `Weapon` 장비 슬롯에 들어갑니다. 장착 도구는 캐릭터의 `HandGrip_R` 소켓에 표시됩니다.
- 곡괭이와 괭이 행의 실제 메시 연결을 교정했습니다. 곡괭이는 기존 `/Hoes/SM_Hoe` 메시, 괭이는 기존 `/Pickaxes/SM_Pickaxe` 메시를 사용합니다.
- 도끼·망치·곡괭이·괭이·삽의 그립 위치, Yaw 90도 회전, 0.55 크기를 아이템 데이터에 반영했습니다. 각 도구의 위치는 요청한 값에 맞춥니다.
- 도끼는 나무에, 곡괭이는 바위에 기본 피해의 1.5배를 줍니다.
- 공격은 `MM_Attack_01`과 `MM_Attack_02`를 번갈아 재생합니다. 공격 중 입력 하나를 예약할 수 있습니다. 양손의 충돌 상자는 같은 공격에서 같은 대상을 한 번만 처리합니다.

## 에디터에서 조정할 위치

| 에셋 | 조정 항목 | 기본값 |
| --- | --- | --- |
| `/Game/Sia/Harvesting/Trees/Profiles/DA_TreeHarvestProfile` | 최대 체력, 도끼 배율, 통나무 수량, 사과 확률과 수량, 넘어지는 시간과 각도 | 체력 100, 도끼 1.5배, 통나무 2–4개, 사과 18% 확률로 1–2개 |
| `/Game/Sia/Harvesting/Rocks/Profiles/DA_RockHarvestProfile` | 최대 체력, 곡괭이 배율, 돌 수량, 타격당 분리 조각 수, Z축 충돌 배율, 파쇄 바리에이션, 사라지는 시간 | 체력 80, 곡괭이 1.5배, 돌 2–4개, 타격당 조각 1개, Z축 1.4배, 3초 |
| 레벨 인스턴스 `BP_TreeResource` 또는 `BP_RockResource` | `Resource Size` | 1.0 |
| `/Game/Data/DT_Items` 및 `Content/Data/Source/Items.json` | 도구 종류, 장비 슬롯, 픽업 메시, 아이템 정의 | 24개 행 |

드롭 확률과 수량은 `FHarvestDropRule`로 관리합니다. 크기 배율은 프로필마다 조정할 수 있습니다. 중복된 아이템 규칙은 하나의 픽업으로 합칩니다.

## 사용 에셋과 정리 위치

| 용도 | 프로젝트 에셋 경로 |
| --- | --- |
| 나무 액터 | `/Game/Sia/Harvesting/Trees/BP_TreeResource` |
| 나무 프로필 | `/Game/Sia/Harvesting/Trees/Profiles/DA_TreeHarvestProfile` |
| 나무 본체 바리에이션 | `/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree01/SM_green-tree01`, `/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree02/SM_green-tree02`, `/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree03/SM_dead-tree03`, `/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree04/SM_dead-tree04`, `/Game/STF/Pack03-LandscapePro/Environment/Foliage/GreenTrees/tree05/SM_dead-tree05` |
| 나무 충돌 | `DA_TreeHarvestProfile`: 몸통 반지름 35cm, 높이 320cm, 플레이어 충돌은 겹침 |
| 통나무 픽업과 작은 충격 조각 | `/Game/Mega_Survival_Tools/Models/SM_Log` |
| 바위 액터 | `/Game/Sia/Harvesting/Rocks/BP_RockResource` |
| 바위 프로필 | `/Game/Sia/Harvesting/Rocks/Profiles/DA_RockHarvestProfile` |
| 바위 본체 바리에이션 | `/Game/RocksPackDEMO/Environment/Rocks/SM_RockPack_RockGroups_01_A`, `/Game/RocksPackDEMO/Environment/Rocks/SM_RockPack_RockGroups_02_B`, `/Game/RocksPackDEMO/Environment/Rocks/SM_RockPack_Rock_07` |
| 바위 Chaos 파쇄 에셋 | `/Game/Sia/Harvesting/Rocks/Destruction/GC_SM_RockPack_RockGroups_01_A`, `/Game/Sia/Harvesting/Rocks/Destruction/GC_SM_RockPack_RockGroups_02_B`, `/Game/Sia/Harvesting/Rocks/Destruction/GC_SM_RockPack_Rock_07` |
| 바위 충돌 | 메시 경계 X/Y 유지, Z에 `Collision Vertical Scale` 적용(기본 1.4) |
| 바위 파괴 조각 | `/Game/Sia/Harvesting/Rocks/Fragments/SM_RockGroup01/SM_RockGroup01_Piece01`부터 `Piece04`까지 |
| 돌 픽업 메시 | `SM_RockGroup01_Piece04` |
| 도구 메시 | `/Game/Sia/Harvesting/Tools/Axes`, `Hammers`, `Pickaxes`, `Hoes`, `Shovels` |
| 도구 원본 묶음 | `/Game/Fab/Survival_Game_Tool_Pack/survival_game_tool_pack_free_agustin_honnun/StaticMeshes/survival_game_tool_pack_free_agustin_honnun` |

사용 에셋과 원본 위치는 `Tools/HarvestAssetManifest.json`, `Tools/HarvestRockGroupManifest.json`, `Tools/HarvestToolExtractionManifest.json`에도 기록했습니다. 추출 스크립트는 원본 묶음을 바꾸지 않고 역할별 메시를 `/Game/Sia/Harvesting` 아래에 만듭니다.

## 구현 위치

- `Source/Sia/Data/HarvestDataTypes.h`와 `.cpp`: 도구·드롭 데이터, 프로필 유효성 검사, 피해와 드롭 수량 계산.
- `Source/Sia/World/HarvestableResource.*`: 공통 체력, 중복 보상 방지, 아이템 픽업 생성.
- `Source/Sia/World/TreeResource.*`: 나무 쓰러짐과 작은 통나무 물리 조각.
- `Source/Sia/World/RockResource.*`와 `HarvestDebris.*`: 바깥 조각 드롭, 변형별 Chaos 파쇄, 충돌 상자, 정리 시간.
- `Source/Sia/BaseCharacter.*`: 양손 공격 판정, 공격 입력 예약, 장비 도구 표시와 자원 타격.
- `Source/Sia/CharacterAnimationComponent.*`: `MM_Attack_01`과 `MM_Attack_02` 재생.
- `Source/Sia/Tests/HarvestingTests.cpp`: 자원·장비·전투 동작 자동화 검사.

## 확인한 항목

- Unreal Engine 5.8 `SiaEditor Win64 Development` 빌드와 격리 프로젝트의 자동화 검사 16개가 모두 통과했습니다. 실패 0개입니다.
- 에셋 검사에서 나무 메시 5개, 요청한 바위 본체 메시 3개, 각 메시와 짝이 맞는 파쇄 Geometry Collection 3개, 나무 차단 충돌, 바위의 체력별 축소와 도구 그립 값을 확인했습니다.
- 나무의 실제 채집 경로에서 통나무 픽업 메시 경계가 바닥 `Z=0`에 놓이고, 나무가 넘어지며, 한 번만 보상하는 것을 확인했습니다.
- 서로 다른 위치를 친 두 바위가 같은 첫 번째 바깥 조각을 냅니다. 바위 파쇄 검사에서 Chaos 조각 분리, 돌 픽업 메시 경계의 바닥 배치, 3초 뒤 액터 제거를 확인했습니다.
- 인벤토리에서 버린 사과, 나무에서 나온 통나무, 바위에서 나온 돌의 메시 경계가 바닥 `Z=0`에 놓이는지 각 경로에서 검사했습니다.
- 오른손 판정을 치운 상태에서 왼손 판정만으로 나무에 피해를 주고, 장착 도끼 배율을 적용하는 것을 확인했습니다.
- 공격 입력 예약이 `MM_Attack_01` 다음 `MM_Attack_02`를 재생하는 것을 확인했습니다.
- 전투 검사에서 장착 도끼가 DataTable의 요청 위치·회전·크기를 적용하는지 확인했습니다.

## 제한과 다음 확인

- 나무는 타격 지점 주변에 작은 물리 조각을 만들고 쓰러집니다. 나무 본체에 타격 지점별 Chaos 파쇄를 적용하는 Geometry Collection은 아직 없습니다.
- 이번 검증은 미저장 `Main` 맵을 보존하려고 격리 프로젝트에서 `-nullrhi` 자동화 검사로 진행했습니다. 뷰포트에서 도끼 위치와 바위 크기를 보는 PIE 시각 확인은 별도로 남아 있습니다.
- 일부 자동화 테스트는 스켈레탈 메시가 없는 임시 캐릭터에서 손 소켓을 조회해 경고를 기록합니다. 검사는 통과하며, 이 경고는 실제 레벨의 소켓 설정을 검증하지 않습니다.
- 자동 검사는 평평한 테스트 바닥에서 메시 경계의 최저점을 비교합니다. 실제 `Main` 레벨 바닥과 다양한 지형에서의 배치는 별도로 플레이 확인이 필요합니다.

레벨에서 확인하려면 콘텐츠 브라우저의 `BP_TreeResource`와 `BP_RockResource`를 배치하고 플레이를 시작하세요. 사과 드롭은 18% 확률이므로 여러 나무를 공격해야 확인할 수 있습니다. 도구 메시의 손 위치나 크기는 `DT_Items`의 `ToolGripTransform`에서 조정합니다.
