# 인벤토리 사용 방법

Ria와 CSTutorial-master의 데이터 정의, 런타임 스택, 변경 알림 구조를 참고했다.
두 참고 프로젝트는 수정하지 않는다. 화면 배치와 디자인은 기존 WBP를 사용한다.

## 에셋

- `/Game/Data/DT_Items`: 아이템 데이터 테이블. 행 구조체는 `FItemData`.
- `/Game/UI/WBP_Inventory`: 장비 영역과 4열 × 6행 인벤토리.
- `/Game/UI/WBP_InventorySlot`: 아이콘, 수량, 선택 표시. 아이콘이 없으면 이름 앞 두 글자를 표시한다.
- `/Game/Items/BP_ItemPickup`: 레벨에 배치하는 아이템. `Item Row`와 `Quantity`를 설정한다.
- `/Game/Items/Icons`: CSTutorial-master의 원본 PNG에서 가져온 물약·양동이 아이콘.

## 데이터 테이블

행 이름이 아이템 ID다. 사과·나무·돌 등 기본 아이템 10종과 머리·몸통·손·다리·발·목걸이·장신구·반지 방어구 8종을 등록했다. 무기는 `ItemType=Weapon`, `EquipmentSlot=Weapon`으로 데이터 행을 추가한다.

| 필드 | 용도 |
| --- | --- |
| ItemType / ItemQuality / EquipmentSlot | 종류 / 등급 / 장착 위치 |
| TextData.DisplayName | 화면에 표시하는 이름 |
| TextData.Description / UsageText | 설명 / 사용 안내 |
| NumericData.Weight | 한 개의 무게 |
| NumericData.MaxStackSize | 한 칸의 최대 수량. 1이면 스택 불가 |
| ItemStatistics.RestorationAmount | 소비 시 체력 회복량 |
| ItemStatistics.DamageValue / ArmorRating / SellValue | 무기·방어구·판매 기능용 정의 값 |
| AssetData.Icon / Mesh | 인벤토리 아이콘 / 월드 메시 |

보유 수량은 테이블이 아닌 캐릭터의 `InventoryComponent`에서 관리한다.
기본 한도는 24칸 / 50.0 무게다. 캐릭터 BP의 InventoryComponent 기본값에서 변경할 수 있다.
최초 지급 아이템이 필요하면 같은 컴포넌트의 `Initial Items`를 설정한다. 기본은 빈 인벤토리다.

원본 정의는 `Content/Data/Source/Items.json`에도 저장했다. JSON을 수정한 뒤
언리얼의 Python 스크립트 실행 메뉴에서 `Tools/ImportItems.py`를 실행하면 테이블에 반영된다.
테이블을 직접 편집한 경우에는 JSON을 다시 적용하면 덮어쓰므로 한쪽을 기준으로 관리한다.
테이블 변경은 새로 획득하는 아이템에 적용하며, 이미 보유한 스택은 획득 당시 정의를 유지한다.

## 플레이와 WBP 연결

1. `BP_ItemPickup`을 캐릭터 가까이에 배치하고 `Item Row`를 지정한다.
2. F키로 줍는다. 무게·빈 슬롯 한도만큼 획득하며, 획득하지 못한 수량은 월드에 남는다.
3. I키로 인벤토리를 연다. 좌클릭 드래그로 칸 이동·병합, 우클릭으로 회복 소비품 사용, 장비 영역에 드래그하거나 더블클릭해 장착한다. 묶음 아이템을 창 밖으로 드래그하면 버릴 수량을 선택하고 확인한다.
4. 인벤토리에서 아이템을 화면 하단 퀵슬롯 1~4에 드래그해 지정하고, 플레이 중 해당 숫자 키로 회복 소비품을 사용한다.
5. 체력이 가득 찼거나 사망한 상태에서는 소비품이 차감되지 않는다.

장비 1개를 획득하면 해당 장비 칸이 비어 있을 때 자동 장착한다. 그 외에는 인벤토리에 보관한다.
장비 교체는 더블클릭으로 처리한다. 장착 중이던 장비는 인벤토리의 빈 칸으로 돌아간다.
해제는 `UnequipItem` Blueprint 함수를 호출한다. 인벤토리에 빈 칸이 없으면 장착/해제가 거부된다.
현재 장비는 인벤토리 컴포넌트가 관리하고, 장비 칸의 이름과 보유 무게는 변경 이벤트로 갱신된다.

`BasePlayerController`가 소유 캐릭터의 인벤토리를 WBP의 `BindInventory`에 연결한다.
`OnInventoryChanged` 이벤트가 발생하면 아이콘·수량·무게·툴팁이 자동 갱신된다.
소유 캐릭터가 바뀌거나 위젯이 제거되면 이전 이벤트 구독을 해제한다.

BP에서는 `Get Inventory → Try Add Item`, `Remove From Slot`, `Remove Item`, `Use Item`,
`Move Or Merge`, `Split Stack`, `Assign Quick Slot`, `Use Quick Slot`을 호출할 수 있다.

개발 콘솔에서 `GiveItem HealthPotion 3` 또는 `GiveItem Apple 25`로 연동을 확인할 수 있다.
무기 장착, 장비 능력치의 전투 반영, 저장/불러오기, 멀티플레이 복제는 별도 기능이다.

퀵슬롯은 같은 아이템의 모든 인벤토리 묶음을 합산해 수량을 표시한다. Escape로 드래그를 취소하면 아이템은 유지된다.
버리기 확인 시 처음 선택한 아이템이 해당 슬롯에 남아 있는지 검사하며, 인벤토리 재연결 시 확인창을 닫는다.

## 검증

자동화 테스트 필터: `Sia.Inventory`

스택 상한, 부분 획득, 무게 제한, 잘못된 입력, 슬롯 유지, 분할·병합,
WBP 자동 갱신/구독 해제, 픽업 잔량/거리, 회복/소비 및 데이터 테이블을 검증한다.
