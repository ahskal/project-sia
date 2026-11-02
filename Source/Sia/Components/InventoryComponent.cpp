#include "Components/InventoryComponent.h"
#include "Items/ItemBase.h"
#include "BaseCharacter.h"
#include "World/ItemPickup.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    ItemDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/Data/DT_Items.DT_Items")));
    EquippedItems.SetNum(static_cast<int32>(EEquipmentSlot::Weapon));
    QuickSlotItemIds.SetNum(4);
}

void UInventoryComponent::EnsureSlots()
{
    if (Slots.IsEmpty()) Slots.SetNum(FMath::Clamp(MaxSlots, 1, 120));
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    // Resize existing component instances when the equipment enum gains another slot.
    EquippedItems.SetNum(static_cast<int32>(EEquipmentSlot::Weapon));
    MaxSlots = FMath::Clamp(MaxSlots, 1, 120);
    MaxWeight = FMath::IsFinite(MaxWeight) ? FMath::Max(0.f, MaxWeight) : 0.f;
    EnsureSlots();
    for (const auto& Entry : InitialItems) TryAddItem(Entry.ItemId, Entry.Quantity);
}

bool UInventoryComponent::InitializeInventory(UDataTable* Table, int32 InMaxSlots, float InMaxWeight)
{
    if (GetOccupiedSlotCount() || EquippedItems.ContainsByPredicate([](const auto& Item) { return Item != nullptr; })
        || !Table || Table->GetRowStruct() != FItemData::StaticStruct()
        || InMaxSlots < 1 || InMaxSlots > 120 || !FMath::IsFinite(InMaxWeight) || InMaxWeight < 0.f) return false;
    ItemDataTable = Table;
    MaxSlots = InMaxSlots;
    MaxWeight = InMaxWeight;
    Slots.SetNum(MaxSlots);
    CommitChange();
    return true;
}

UDataTable* UInventoryComponent::GetItemDataTable() const
{
    UDataTable* Table = ItemDataTable.LoadSynchronous();
    return Table && Table->GetRowStruct() == FItemData::StaticStruct() ? Table : nullptr;
}

UItemBase* UInventoryComponent::CreateStack(FName ItemId, const FItemData& Data, int32 Quantity)
{
    auto* Item = NewObject<UItemBase>(this);
    Item->ItemId = ItemId;
    Item->Data = Data;
    Item->Quantity = Quantity;
    return Item;
}

FItemAddResult UInventoryComponent::TryAddItem(FName ItemId, int32 Quantity)
{
    FItemAddResult Result;
    Result.AmountRequested = Quantity;
    UDataTable* Table = GetItemDataTable();
    const FItemData* Row = Table && !ItemId.IsNone() ? Table->FindRow<FItemData>(ItemId, TEXT("Inventory"), false) : nullptr;
    if (Quantity <= 0 || !Row || !Row->IsValidDefinition())
    {
        Result.Message = FText::FromString(TEXT("아이템 ID, 수량 또는 데이터 테이블 설정을 확인하세요."));
        return Result;
    }
    EnsureSlots();
    const bool bEquipmentItem = Row->ItemType == EItemType::Armor || Row->ItemType == EItemType::Weapon;
    const int32 EquipmentIndex = static_cast<int32>(Row->EquipmentSlot) - 1;
    if (bEquipmentItem && Quantity == 1 && Row->EquipmentSlot != EEquipmentSlot::None
        && EquippedItems.IsValidIndex(EquipmentIndex) && !EquippedItems[EquipmentIndex]
        && CurrentWeight + Row->NumericData.Weight <= MaxWeight)
    {
        EquippedItems[EquipmentIndex] = CreateStack(ItemId, *Row, 1);
        Result.AmountAdded = 1;
        Result.Result = EItemAddResult::All;
        Result.Message = FText::Format(FText::FromString(TEXT("{0} 장비 칸에 보관")), Row->TextData.DisplayName);
        CommitChange();
        return Result;
    }
    // 스택이 보유 중일 때에는 처음 획득한 정의를 유지한다. 에디터에서 테이블을 바꾸어도
    // 기존 스택과 새 스택의 단위 무게/상한이 달라지는 것을 방지한다.
    const FItemData* Definition = Row;
    for (const auto& Item : Slots)
        if (Item && Item->ItemId == ItemId) { Definition = &Item->Data; break; }
    const int32 StackLimit = Definition->NumericData.MaxStackSize;
    const double UnitWeight = Definition->NumericData.Weight;
    int32 Allowed = Quantity;
    if (UnitWeight > 0.0)
    {
        const double WeightLimit = FMath::FloorToDouble(FMath::Max(0.0, double(MaxWeight) - CurrentWeight) / UnitWeight + 1.e-6);
        Allowed = static_cast<int32>(FMath::Min(double(Quantity), WeightLimit));
    }
    int32 Remaining = Allowed;
    for (auto& Item : Slots)
    {
        if (!Item || Item->ItemId != ItemId || Remaining == 0) continue;
        const int32 Added = FMath::Min(Remaining, StackLimit - Item->Quantity);
        Item->Quantity += Added;
        Remaining -= Added;
    }
    for (auto& Item : Slots)
    {
        if (Item || Remaining == 0) continue;
        const int32 Added = FMath::Min(Remaining, StackLimit);
        Item = CreateStack(ItemId, *Definition, Added);
        Remaining -= Added;
    }
    Result.AmountAdded = Allowed - Remaining;
    Result.Result = Result.AmountAdded == Quantity ? EItemAddResult::All
        : Result.AmountAdded > 0 ? EItemAddResult::Partial : EItemAddResult::None;
    Result.Message = Result.Result == EItemAddResult::All
        ? FText::Format(FText::FromString(TEXT("{0} {1}개 획득")), Definition->TextData.DisplayName, FText::AsNumber(Result.AmountAdded))
        : FText::Format(FText::FromString(TEXT("{0}개 획득 / 무게 또는 빈 슬롯이 부족합니다.")), FText::AsNumber(Result.AmountAdded));
    if (Result.AmountAdded > 0) CommitChange();
    return Result;
}

void UInventoryComponent::CommitChange()
{
    CurrentWeight = 0.0;
    for (const auto& Item : Slots) if (Item) CurrentWeight += Item->GetStackWeight();
    for (const auto& Item : EquippedItems) if (Item) CurrentWeight += Item->GetStackWeight();
    OnInventoryChanged.Broadcast();
}

UItemBase* UInventoryComponent::GetItemAt(int32 SlotIndex) const
{
    return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

int32 UInventoryComponent::GetOccupiedSlotCount() const
{
    int32 Count = 0;
    for (const auto& Item : Slots) if (Item) ++Count;
    return Count;
}

int32 UInventoryComponent::GetTotalQuantity(FName ItemId) const
{
    int64 Total = 0;
    for (const auto& Item : Slots) if (Item && Item->ItemId == ItemId) Total += Item->Quantity;
    return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

int32 UInventoryComponent::RemoveFromSlot(int32 SlotIndex, int32 Quantity)
{
    UItemBase* Item = GetItemAt(SlotIndex);
    if (!Item || Quantity <= 0) return 0;
    const int32 Removed = FMath::Min(Quantity, Item->Quantity);
    Item->Quantity -= Removed;
    if (!Item->Quantity) Slots[SlotIndex] = nullptr;
    CommitChange();
    return Removed;
}

bool UInventoryComponent::DropItemFromSlot(int32 SlotIndex, int32 Quantity)
{
    UItemBase* Item = GetItemAt(SlotIndex);
    ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
    if (!Item || !Character || Quantity <= 0 || !GetWorld()) return false;
    const int32 DropQuantity = FMath::Min(Quantity, Item->GetQuantity());
    const FVector SpawnLocation = Character->GetActorLocation()
        + Character->GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 35.f);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AItemPickup* Pickup = GetWorld()->SpawnActor<AItemPickup>(SpawnLocation, FRotator::ZeroRotator, Params);
    if (!Pickup) return false;
    Pickup->ItemRow.DataTable = GetItemDataTable();
    Pickup->ItemRow.RowName = Item->GetItemId();
    Pickup->Quantity = DropQuantity;
    Pickup->RefreshForInventoryDrop();
    RemoveFromSlot(SlotIndex, DropQuantity);
    return true;
}

bool UInventoryComponent::DropEquippedItem(EEquipmentSlot EquipmentSlot)
{
    const int32 EquipmentIndex = static_cast<int32>(EquipmentSlot) - 1;
    UItemBase* Item = EquippedItems.IsValidIndex(EquipmentIndex) ? EquippedItems[EquipmentIndex].Get() : nullptr;
    ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwner());
    if (!Item || !Character || !GetWorld()) return false;

    const FVector SpawnLocation = Character->GetActorLocation()
        + Character->GetActorForwardVector() * 120.f + FVector(0.f, 0.f, 35.f);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AItemPickup* Pickup = GetWorld()->SpawnActor<AItemPickup>(SpawnLocation, FRotator::ZeroRotator, Params);
    if (!Pickup) return false;

    Pickup->ItemRow.DataTable = GetItemDataTable();
    Pickup->ItemRow.RowName = Item->GetItemId();
    Pickup->Quantity = Item->GetQuantity();
    Pickup->RefreshForInventoryDrop();
    EquippedItems[EquipmentIndex] = nullptr;
    CommitChange();
    return true;
}

int32 UInventoryComponent::RemoveItem(FName ItemId, int32 Quantity)
{
    if (Quantity <= 0) return 0;
    int32 Remaining = Quantity;
    for (int32 Index = Slots.Num() - 1; Index >= 0 && Remaining > 0; --Index)
    {
        auto& Item = Slots[Index];
        if (!Item || Item->ItemId != ItemId) continue;
        const int32 Removed = FMath::Min(Remaining, Item->Quantity);
        Item->Quantity -= Removed;
        Remaining -= Removed;
        if (!Item->Quantity) Item = nullptr;
    }
    if (Remaining < Quantity) CommitChange();
    return Quantity - Remaining;
}

bool UInventoryComponent::MoveOrMerge(int32 FromSlot, int32 ToSlot)
{
    UItemBase* From = GetItemAt(FromSlot);
    if (!From || FromSlot == ToSlot || !Slots.IsValidIndex(ToSlot)) return false;
    UItemBase* To = GetItemAt(ToSlot);
    if (To && From->ItemId == To->ItemId)
    {
        const int32 Amount = FMath::Min(From->Quantity, To->Data.NumericData.MaxStackSize - To->Quantity);
        if (Amount <= 0) return false;
        To->Quantity += Amount;
        From->Quantity -= Amount;
        if (!From->Quantity) Slots[FromSlot] = nullptr;
    }
    else Slots.Swap(FromSlot, ToSlot);
    CommitChange();
    return true;
}

bool UInventoryComponent::SplitStack(int32 SlotIndex, int32 Quantity)
{
    UItemBase* Item = GetItemAt(SlotIndex);
    if (!Item || Quantity <= 0 || Quantity >= Item->Quantity) return false;
    const int32 Empty = Slots.IndexOfByPredicate([](const auto& Entry) { return !Entry; });
    if (Empty == INDEX_NONE) return false;
    Slots[Empty] = CreateStack(Item->ItemId, Item->Data, Quantity);
    Item->Quantity -= Quantity;
    CommitChange();
    return true;
}

bool UInventoryComponent::UseItem(int32 SlotIndex, FText& OutMessage)
{
    UItemBase* Item = GetItemAt(SlotIndex);
    auto* Character = Cast<ABaseCharacter>(GetOwner());
    if (!Item || !Character || Item->Data.ItemType != EItemType::Consumable
        || Item->Data.ItemStatistics.RestorationAmount <= 0.f)
    {
        OutMessage = FText::FromString(TEXT("사용 가능한 회복 소비품이 아닙니다."));
        return false;
    }
    const float Restored = Character->RestoreHealth(Item->Data.ItemStatistics.RestorationAmount);
    if (Restored <= 0.f)
    {
        OutMessage = FText::FromString(TEXT("체력이 가득 찼거나 사용할 수 없는 상태입니다."));
        return false;
    }
    OutMessage = FText::Format(FText::FromString(TEXT("{0} 사용 · 체력 +{1}")),
        Item->Data.TextData.DisplayName, FText::AsNumber(FMath::RoundToInt(Restored)));
    RemoveFromSlot(SlotIndex, 1);
    return true;
}

bool UInventoryComponent::AssignQuickSlot(int32 QuickSlotIndex, int32 InventorySlot)
{
    UItemBase* Item = GetItemAt(InventorySlot);
    if (!QuickSlotItemIds.IsValidIndex(QuickSlotIndex) || !Item) return false;
    QuickSlotItemIds[QuickSlotIndex] = Item->GetItemId();
    OnInventoryChanged.Broadcast();
    return true;
}

FName UInventoryComponent::GetQuickSlotItemId(int32 QuickSlotIndex) const
{
    return QuickSlotItemIds.IsValidIndex(QuickSlotIndex) ? QuickSlotItemIds[QuickSlotIndex] : NAME_None;
}

bool UInventoryComponent::UseQuickSlot(int32 QuickSlotIndex, FText& OutMessage)
{
    const FName ItemId = GetQuickSlotItemId(QuickSlotIndex);
    if (ItemId.IsNone())
    {
        OutMessage = FText::FromString(TEXT("퀵슬롯이 비어 있습니다."));
        return false;
    }
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (Slots[Index] && Slots[Index]->GetItemId() == ItemId)
            return UseItem(Index, OutMessage);
    }
    OutMessage = FText::FromString(TEXT("퀵슬롯에 지정한 아이템이 인벤토리에 없습니다."));
    return false;
}

UItemBase* UInventoryComponent::GetEquippedItem(EEquipmentSlot EquipmentSlot) const
{
    const int32 Index = static_cast<int32>(EquipmentSlot) - 1;
    return EquippedItems.IsValidIndex(Index) ? EquippedItems[Index].Get() : nullptr;
}

bool UInventoryComponent::EquipItem(int32 InventorySlot, FText& OutMessage)
{
    UItemBase* Item = GetItemAt(InventorySlot);
    if (!Item)
    {
        OutMessage = FText::FromString(TEXT("인벤토리 슬롯이 비어 있습니다."));
        return false;
    }

    const EEquipmentSlot ItemSlot = Item->Data.EquipmentSlot;
    const bool bCanEquipArmor = Item->Data.ItemType == EItemType::Armor
        && ItemSlot != EEquipmentSlot::None && ItemSlot != EEquipmentSlot::Weapon;
    const bool bCanEquipWeapon = Item->Data.ItemType == EItemType::Weapon && ItemSlot == EEquipmentSlot::Weapon;
    if (!bCanEquipArmor && !bCanEquipWeapon)
    {
        OutMessage = FText::FromString(TEXT("아이템 종류와 장비 슬롯이 맞지 않습니다."));
        return false;
    }

    const int32 EquipmentIndex = static_cast<int32>(Item->Data.EquipmentSlot) - 1;
    if (!EquippedItems.IsValidIndex(EquipmentIndex))
    {
        OutMessage = FText::FromString(TEXT("장비 슬롯 설정이 올바르지 않습니다."));
        return false;
    }

    UItemBase* Previous = EquippedItems[EquipmentIndex];
    const int32 EmptySlot = Slots.IndexOfByPredicate([](const auto& Entry) { return !Entry; });
    if (Previous && Item->Quantity > 1 && EmptySlot == INDEX_NONE)
    {
        OutMessage = FText::FromString(TEXT("교체할 장비를 넣을 인벤토리 칸이 없습니다."));
        return false;
    }

    UItemBase* Equipped = CreateStack(Item->ItemId, Item->Data, 1);
    --Item->Quantity;
    if (!Item->Quantity) Slots[InventorySlot] = nullptr;
    EquippedItems[EquipmentIndex] = Equipped;
    if (Previous)
    {
        const int32 ReturnSlot = Item->Quantity == 0 ? InventorySlot : EmptySlot;
        Slots[ReturnSlot] = Previous;
    }

    OutMessage = FText::Format(FText::FromString(TEXT("{0} 장착")), Item->Data.TextData.DisplayName);
    CommitChange();
    return true;
}

bool UInventoryComponent::EquipItemToSlot(int32 InventorySlot, EEquipmentSlot EquipmentSlot, FText& OutMessage)
{
    UItemBase* Item = GetItemAt(InventorySlot);
    if (!Item)
    {
        OutMessage = FText::FromString(TEXT("인벤토리 슬롯이 비어 있습니다."));
        return false;
    }

    if (Item->GetData().EquipmentSlot != EquipmentSlot)
    {
        OutMessage = FText::FromString(TEXT("아이템 데이터와 장비 칸이 일치하지 않습니다."));
        return false;
    }

    return EquipItem(InventorySlot, OutMessage);
}

bool UInventoryComponent::UnequipItem(EEquipmentSlot EquipmentSlot, FText& OutMessage)
{
    const int32 EquipmentIndex = static_cast<int32>(EquipmentSlot) - 1;
    if (!EquippedItems.IsValidIndex(EquipmentIndex) || !EquippedItems[EquipmentIndex])
    {
        OutMessage = FText::FromString(TEXT("해당 장비 슬롯이 비어 있습니다."));
        return false;
    }
    const int32 EmptySlot = Slots.IndexOfByPredicate([](const auto& Entry) { return !Entry; });
    if (EmptySlot == INDEX_NONE)
    {
        OutMessage = FText::FromString(TEXT("인벤토리에 빈 칸이 없습니다."));
        return false;
    }
    UItemBase* Item = EquippedItems[EquipmentIndex];
    Slots[EmptySlot] = Item;
    EquippedItems[EquipmentIndex] = nullptr;
    OutMessage = FText::Format(FText::FromString(TEXT("{0} 해제")), Item->Data.TextData.DisplayName);
    CommitChange();
    return true;
}

bool UInventoryComponent::UnequipItemToSlot(EEquipmentSlot EquipmentSlot, int32 InventorySlot, FText& OutMessage)
{
    const int32 EquipmentIndex = static_cast<int32>(EquipmentSlot) - 1;
    if (!EquippedItems.IsValidIndex(EquipmentIndex) || !EquippedItems[EquipmentIndex])
    {
        OutMessage = FText::FromString(TEXT("해당 장비 슬롯이 비어 있습니다."));
        return false;
    }
    if (!Slots.IsValidIndex(InventorySlot))
    {
        OutMessage = FText::FromString(TEXT("인벤토리 칸을 찾을 수 없습니다."));
        return false;
    }

    UItemBase* Item = EquippedItems[EquipmentIndex];
    UItemBase* TargetItem = Slots[InventorySlot];
    if (!TargetItem)
    {
        Slots[InventorySlot] = Item;
        EquippedItems[EquipmentIndex] = nullptr;
        OutMessage = FText::Format(FText::FromString(TEXT("{0} 장비 해제")), Item->GetData().TextData.DisplayName);
        CommitChange();
        return true;
    }

    if (TargetItem->GetItemId() == Item->GetItemId()
        && TargetItem->GetQuantity() < TargetItem->GetData().NumericData.MaxStackSize)
    {
        ++TargetItem->Quantity;
        EquippedItems[EquipmentIndex] = nullptr;
        OutMessage = FText::Format(FText::FromString(TEXT("{0} 장비를 인벤토리 묶음에 합쳤습니다.")),
            Item->GetData().TextData.DisplayName);
        CommitChange();
        return true;
    }

    const bool bTargetCanReplaceEquipment = TargetItem->GetData().EquipmentSlot == EquipmentSlot
        && ((TargetItem->GetData().ItemType == EItemType::Armor && EquipmentSlot != EEquipmentSlot::Weapon)
            || (TargetItem->GetData().ItemType == EItemType::Weapon && EquipmentSlot == EEquipmentSlot::Weapon));
    if (!bTargetCanReplaceEquipment)
    {
        OutMessage = FText::FromString(TEXT("목표 칸의 아이템은 이 장비 슬롯에 장착할 수 없습니다."));
        return false;
    }

    const int32 EmptySlot = Slots.IndexOfByPredicate([](const auto& Entry) { return !Entry; });
    const bool bTargetStackNeedsRoom = TargetItem->GetQuantity() > 1;
    if (bTargetStackNeedsRoom && EmptySlot == INDEX_NONE)
    {
        OutMessage = FText::FromString(TEXT("장비 묶음의 나머지를 옮길 인벤토리 칸이 없습니다."));
        return false;
    }

    if (bTargetStackNeedsRoom)
    {
        --TargetItem->Quantity;
        Slots[EmptySlot] = TargetItem;
        EquippedItems[EquipmentIndex] = CreateStack(TargetItem->GetItemId(), TargetItem->GetData(), 1);
    }
    else
    {
        EquippedItems[EquipmentIndex] = TargetItem;
    }

    Slots[InventorySlot] = Item;
    OutMessage = FText::Format(FText::FromString(TEXT("{0} 장비를 {1} 장비와 교체했습니다.")),
        Item->GetData().TextData.DisplayName,
        EquippedItems[EquipmentIndex]->GetData().TextData.DisplayName);
    CommitChange();
    return true;
}
