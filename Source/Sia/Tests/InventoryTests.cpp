#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Components/InventoryComponent.h"
#include "Components/TextBlock.h"
#include "Items/ItemBase.h"
#include "InventoryWidget.h"
#include "CombatHUDWidget.h"
#include "Components/Border.h"
#include "BaseCharacter.h"
#include "World/ItemPickup.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include <limits>

namespace InventoryTests
{
    UDataTable* Table()
    {
        auto* Result = NewObject<UDataTable>();
        Result->RowStruct = FItemData::StaticStruct();
        FItemData Apple;
        Apple.TextData.DisplayName = FText::FromString(TEXT("테스트 사과"));
        Apple.ItemType = EItemType::Consumable;
        Apple.ItemStatistics.RestorationAmount = 30.f;
        Apple.NumericData.MaxStackSize = 10;
        Apple.NumericData.Weight = 1.f;
        Result->AddRow(TEXT("Apple"), Apple);
        FItemData Free = Apple;
        Free.NumericData.Weight = 0.f;
        Result->AddRow(TEXT("Free"), Free);
        FItemData Sword = Apple;
        Sword.ItemType = EItemType::Weapon;
        Sword.NumericData.MaxStackSize = 1;
        Result->AddRow(TEXT("Sword"), Sword);
        FItemData Potion = Apple;
        Potion.NumericData.Weight = .5f;
        Result->AddRow(TEXT("Potion"), Potion);
        FItemData Helmet = Apple;
        Helmet.ItemType = EItemType::Armor;
        Helmet.EquipmentSlot = EEquipmentSlot::Head;
        Helmet.ItemStatistics.ArmorRating = 8.f;
        Helmet.NumericData.Weight = 2.f;
        Helmet.NumericData.MaxStackSize = 1;
        Result->AddRow(TEXT("Helmet"), Helmet);
        return Result;
    }
    UInventoryComponent* Inventory(UDataTable* Data, int32 Slots = 24, float Weight = 1000.f)
    {
        auto* Result = NewObject<UInventoryComponent>();
        Result->InitializeInventory(Data, Slots, Weight);
        return Result;
    }
    constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryStackTest, "Sia.Inventory.StacksAndStableSlots", InventoryTests::Flags)
bool FInventoryStackTest::RunTest(const FString&)
{
    auto* Inv = InventoryTests::Inventory(InventoryTests::Table(), 2);
    const FItemAddResult Added = Inv->TryAddItem(TEXT("Apple"), 25);
    TestEqual(TEXT("slot capacity allows two stacks"), Added.AmountAdded, 20);
    TestTrue(TEXT("partial result"), Added.Result == EItemAddResult::Partial);
    UItemBase* Second = Inv->GetItemAt(1);
    TestEqual(TEXT("remove one full stack"), Inv->RemoveFromSlot(0, 10), 10);
    TestNull(TEXT("removed slot remains empty"), Inv->GetItemAt(0));
    TestTrue(TEXT("other slot keeps identity and index"), Inv->GetItemAt(1) == Second);
    TestEqual(TEXT("refill empty slot"), Inv->TryAddItem(TEXT("Apple"), 5).AmountAdded, 5);
    TestEqual(TEXT("quantity in refilled slot"), Inv->GetItemAt(0)->GetQuantity(), 5);
    TestEqual(TEXT("total weight"), Inv->GetCurrentWeight(), 15.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWeightTest, "Sia.Inventory.WeightAndInvalidInput", InventoryTests::Flags)
bool FInventoryWeightTest::RunTest(const FString&)
{
    auto* Data = InventoryTests::Table();
    auto* Inv = InventoryTests::Inventory(Data, 2, 2.5f);
    TestEqual(TEXT("partial add under weight limit"), Inv->TryAddItem(TEXT("Apple"), 8).AmountAdded, 2);
    TestEqual(TEXT("overweight add rejected"), Inv->TryAddItem(TEXT("Apple"), 1).AmountAdded, 0);
    TestEqual(TEXT("negative quantity rejected"), Inv->TryAddItem(TEXT("Apple"), -1).AmountAdded, 0);
    TestEqual(TEXT("unknown row rejected"), Inv->TryAddItem(TEXT("Missing"), 1).AmountAdded, 0);
    TestFalse(TEXT("occupied inventory cannot be reset"), Inv->InitializeInventory(Data, 1, 0.f));
    auto* Zero = InventoryTests::Inventory(Data, 2, 0.f);
    TestEqual(TEXT("positive weight rejected with zero capacity"), Zero->TryAddItem(TEXT("Apple"), 1).AmountAdded, 0);
    TestEqual(TEXT("weightless items still obey slot capacity"), Zero->TryAddItem(TEXT("Free"), 25).AmountAdded, 20);
    FItemData Bad = *Data->FindRow<FItemData>(TEXT("Apple"), TEXT("Test"));
    Bad.NumericData.Weight = std::numeric_limits<float>::quiet_NaN();
    Data->AddRow(TEXT("NaN"), Bad);
    Bad.NumericData.Weight = 1.f;
    Bad.NumericData.MaxStackSize = 0;
    Data->AddRow(TEXT("BadStack"), Bad);
    TestEqual(TEXT("NaN rejected"), Inv->TryAddItem(TEXT("NaN"), 1).AmountAdded, 0);
    TestEqual(TEXT("zero max stack rejected"), Inv->TryAddItem(TEXT("BadStack"), 1).AmountAdded, 0);
    Bad.NumericData.Weight = 1.e-30f;
    Bad.NumericData.MaxStackSize = 10;
    Data->AddRow(TEXT("Tiny"), Bad);
    auto* Tiny = InventoryTests::Inventory(Data, 2, 1.e30f);
    TestEqual(TEXT("huge request and tiny weight do not overflow"), Tiny->TryAddItem(TEXT("Tiny"), MAX_int32).AmountAdded, 20);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryMoveTest, "Sia.Inventory.SplitMergeAndNonStackable", InventoryTests::Flags)
bool FInventoryMoveTest::RunTest(const FString&)
{
    auto* Inv = InventoryTests::Inventory(InventoryTests::Table());
    Inv->TryAddItem(TEXT("Apple"), 15);
    TestTrue(TEXT("split four into empty slot"), Inv->SplitStack(0, 4));
    TestEqual(TEXT("source after split"), Inv->GetItemAt(0)->GetQuantity(), 6);
    TestTrue(TEXT("merge split stack"), Inv->MoveOrMerge(2, 1));
    TestEqual(TEXT("merged quantity"), Inv->GetItemAt(1)->GetQuantity(), 9);
    TestNull(TEXT("merged source cleared"), Inv->GetItemAt(2));
    TestTrue(TEXT("move to an empty slot"), Inv->MoveOrMerge(1, 5));
    TestEqual(TEXT("weight preserved across moves"), Inv->GetCurrentWeight(), 15.0);
    TestFalse(TEXT("cannot split the whole stack"), Inv->SplitStack(5, 9));
    TestEqual(TEXT("remove by ID across slots"), Inv->RemoveItem(TEXT("Apple"), 12), 12);
    TestEqual(TEXT("remaining total"), Inv->GetTotalQuantity(TEXT("Apple")), 3);
    TestEqual(TEXT("three swords added"), Inv->TryAddItem(TEXT("Sword"), 3).AmountAdded, 3);
    TestEqual(TEXT("each nonstackable item uses one slot"), Inv->GetOccupiedSlotCount(), 4);
    TestEqual(TEXT("helmet added"), Inv->TryAddItem(TEXT("Helmet"), 1).AmountAdded, 1);
    FText EquipmentMessage;
    TestEqual(TEXT("automatic equip does not occupy a bag slot"), Inv->GetOccupiedSlotCount(), 4);
    auto* EquippedHelmet = Inv->GetEquippedItem(EEquipmentSlot::Head);
    TestNotNull(TEXT("equipment slot contains helmet"), EquippedHelmet);
    if (EquippedHelmet) TestEqual(TEXT("equipment slot stores helmet"), EquippedHelmet->GetItemId(), FName(TEXT("Helmet")));
    TestEqual(TEXT("equipped weight remains carried"), Inv->GetCurrentWeight(), 8.0);
    TestTrue(TEXT("helmet can be unequipped to an empty slot"), Inv->UnequipItem(EEquipmentSlot::Head, EquipmentMessage));
    TestNull(TEXT("head equipment slot clears"), Inv->GetEquippedItem(EEquipmentSlot::Head));
    TestTrue(TEXT("manual action equips unequipped helmet"), Inv->EquipItem(4, EquipmentMessage));
    TestTrue(TEXT("manual equip retains helmet data"), Inv->GetEquippedItem(EEquipmentSlot::Head)
        && Inv->GetEquippedItem(EEquipmentSlot::Head)->GetItemId() == FName(TEXT("Helmet")));
    TestEqual(TEXT("manual equip preserves weight"), Inv->GetCurrentWeight(), 8.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryWidgetTest, "Sia.Inventory.WBPAutomaticRefresh", InventoryTests::Flags)
bool FInventoryWidgetTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UInventoryWidget>(nullptr, TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C"));
    if (!TestNotNull(TEXT("inventory WBP class"), Class)) return false;
    TStrongObjectPtr<UInventoryWidget> Widget(NewObject<UInventoryWidget>(GetTransientPackage(), Class));
    if (!TestTrue(TEXT("WBP initialized"), Widget->Initialize())) return false;
    auto* Inv = InventoryTests::Inventory(InventoryTests::Table());
    Widget->BindInventory(Inv);
    auto* Capacity = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("CapacityText")));
    if (!TestNotNull(TEXT("capacity BindWidget"), Capacity)) return false;
    TestEqual(TEXT("initially empty"), Capacity->GetText().ToString(), FString(TEXT("0 / 24")));
    Inv->TryAddItem(TEXT("Apple"), 12);
    TestEqual(TEXT("delegate updates WBP immediately"), Capacity->GetText().ToString(), FString(TEXT("2 / 24")));
    auto* Slot = Cast<UInventorySlotWidget>(Widget->GetWidgetFromName(TEXT("InventorySlot_01")));
    auto* Quantity = Slot ? Cast<UTextBlock>(Slot->GetWidgetFromName(TEXT("QuantityText"))) : nullptr;
    if (!TestNotNull(TEXT("designer slot quantity"), Quantity)) return false;
    TestEqual(TEXT("stack value shown"), Quantity->GetText().ToString(), FString(TEXT("10")));
    Inv->RemoveFromSlot(0, 1);
    TestEqual(TEXT("removal updates existing WBP slot"), Quantity->GetText().ToString(), FString(TEXT("9")));
    auto* Other = InventoryTests::Inventory(InventoryTests::Table());
    Widget->BindInventory(Other);
    Inv->TryAddItem(TEXT("Apple"), 10);
    TestEqual(TEXT("old inventory is unsubscribed"), Capacity->GetText().ToString(), FString(TEXT("0 / 24")));
    Widget->BindInventory(nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryPickupTest, "Sia.Inventory.PickupAndConsumption", InventoryTests::Flags)
bool FInventoryPickupTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("isolated test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Character = World->SpawnActorDeferred<ABaseCharacter>(ABaseCharacter::StaticClass(), FTransform::Identity,
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("test character"), Character)) return false;
    Character->GetMesh()->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")));
    Character->FinishSpawning(FTransform::Identity);
    auto* Data = InventoryTests::Table();
    auto* Inv = Character->GetInventory();
    Inv->InitializeInventory(Data, 24, 2.5f);
    const FTransform Transform(FVector(100, 0, 0));
    auto* Pickup = World->SpawnActorDeferred<AItemPickup>(AItemPickup::StaticClass(), Transform);
    if (!TestNotNull(TEXT("test pickup"), Pickup)) return false;
    Pickup->ItemRow.DataTable = Data;
    Pickup->ItemRow.RowName = TEXT("Apple");
    Pickup->Quantity = 25;
    Pickup->FinishSpawning(Transform);
    TestEqual(TEXT("pickup adds only available weight"), Pickup->TryPickup(Character), 2);
    TestEqual(TEXT("partial pickup preserves world remainder"), Pickup->Quantity, 23);
    TestEqual(TEXT("full inventory adds nothing"), Pickup->TryPickup(Character), 0);
    Inv->RemoveItem(TEXT("Apple"), 2);
    Character->SetActorLocation(FVector(1000, 0, 0));
    TestEqual(TEXT("out of range pickup rejected"), Pickup->TryPickup(Character), 0);
    Inv->InitializeInventory(Data, 24, 50.f);
    Inv->TryAddItem(TEXT("Potion"), 2);
    FText Message;
    TestFalse(TEXT("full health does not waste a potion"), Inv->UseItem(0, Message));
    TestEqual(TEXT("full health preserves quantity"), Inv->GetTotalQuantity(TEXT("Potion")), 2);
    // 이 격리 월드는 GameMode가 없어 APawn이 피해를 거부하므로, 회복 테스트의 초기 체력만 설정한다.
    FindFProperty<FFloatProperty>(ABaseCharacter::StaticClass(), TEXT("Health"))->SetPropertyValue_InContainer(Character, 60.f);
    TestTrue(TEXT("damaged character can use potion"), Inv->UseItem(0, Message));
    TestEqual(TEXT("health restored"), Character->GetHealth(), 90.f);
    TestEqual(TEXT("one potion consumed"), Inv->GetTotalQuantity(TEXT("Potion")), 1);
    TestEqual(TEXT("weight updated after consumption"), Inv->GetCurrentWeight(), .5);
    TestTrue(TEXT("last potion used"), Inv->UseItem(0, Message));
    TestEqual(TEXT("healing capped by max health"), Character->GetHealth(), 100.f);
    TestNull(TEXT("empty stack removed"), Inv->GetItemAt(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryDataTableTest, "Sia.Inventory.DataTableDefinitions", InventoryTests::Flags)
bool FInventoryDataTableTest::RunTest(const FString&)
{
    auto* Data = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Items.DT_Items"));
    if (!TestNotNull(TEXT("native item DataTable asset"), Data)) return false;
    TestEqual(TEXT("sample item rows"), Data->GetRowNames().Num(), 18);
    for (const FName Id : Data->GetRowNames())
    {
        const FItemData* Row = Data->FindRow<FItemData>(Id, TEXT("Test"));
        TestTrue(FString::Printf(TEXT("%s definition is valid"), *Id.ToString()), Row && Row->IsValidDefinition());
    }
    const auto* Potion = Data->FindRow<FItemData>(TEXT("HealthPotion"), TEXT("Test"));
    const auto* Bucket = Data->FindRow<FItemData>(TEXT("Bucket"), TEXT("Test"));
    TestTrue(TEXT("potion icon resolves inside Sia"), Potion && Potion->AssetData.Icon.LoadSynchronous());
    TestTrue(TEXT("bucket icon resolves inside Sia"), Bucket && Bucket->AssetData.Icon.LoadSynchronous());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryDragCancellationTest, "Sia.Inventory.DragCancellationPreservesItems", InventoryTests::Flags)
bool FInventoryDragCancellationTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Character = World->SpawnActor<ABaseCharacter>();
    if (!TestNotNull(TEXT("character"), Character)) return false;
    auto* Inv = Character->GetInventory();
    Inv->InitializeInventory(InventoryTests::Table());
    Inv->TryAddItem(TEXT("Apple"), 1);
    Inv->TryAddItem(TEXT("Helmet"), 1);
    auto* Slot = NewObject<UInventorySlotWidget>();
    auto* Drag = NewObject<UInventoryItemDragOperation>();
    Drag->Inventory = Inv;
    Drag->SlotIndex = 0;
    const FDragDropEvent CancelEvent(FPointerEvent(), nullptr);
    Slot->NativeOnDragCancelled(CancelEvent, Drag);
    TestEqual(TEXT("cancel preserves bag item"), Inv->GetTotalQuantity(TEXT("Apple")), 1);
    Drag->bFromEquipment = true;
    Drag->EquipmentSlot = EEquipmentSlot::Head;
    Slot->NativeOnDragCancelled(CancelEvent, Drag);
    TestNotNull(TEXT("cancel preserves equipment"), Inv->GetEquippedItem(EEquipmentSlot::Head));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryDropSafetyTest, "Sia.Inventory.DropConfirmationIdentity", InventoryTests::Flags)
bool FInventoryDropSafetyTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UInventoryWidget>(nullptr, TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C"));
    if (!TestNotNull(TEXT("class"), Class)) return false;
    TStrongObjectPtr<UInventoryWidget> Widget(NewObject<UInventoryWidget>(GetTransientPackage(), Class));
    if (!TestTrue(TEXT("initialize"), Widget->Initialize())) return false;
    auto* Inv = InventoryTests::Inventory(InventoryTests::Table());
    Inv->TryAddItem(TEXT("Apple"), 2);
    Inv->TryAddItem(TEXT("Potion"), 2);
    Widget->BindInventory(Inv);
    Widget->RequestDropItemForSlot(0);
    TestTrue(TEXT("original identity captured"), Widget->PendingDropItem.Get() == Inv->GetItemAt(0));
    Inv->MoveOrMerge(0, 1);
    Widget->ConfirmDropItem();
    TestTrue(TEXT("replacement closes dialog"), Widget->WBP_DropBackdrop->GetVisibility() == ESlateVisibility::Collapsed);
    TestEqual(TEXT("replacement preserved"), Inv->GetTotalQuantity(TEXT("Potion")), 2);
    Widget->RequestDropItemForSlot(0);
    Widget->BindInventory(InventoryTests::Inventory(InventoryTests::Table()));
    TestFalse(TEXT("rebind clears identity"), Widget->PendingDropItem.IsValid());
    TestTrue(TEXT("rebind closes dialog"), Widget->WBP_DropBackdrop->GetVisibility() == ESlateVisibility::Collapsed);
    Widget->BindInventory(nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryQuickSlotTotalTest, "Sia.Inventory.QuickSlotTotalQuantity", InventoryTests::Flags)
bool FInventoryQuickSlotTotalTest::RunTest(const FString&)
{
    UClass* Class = LoadClass<UCombatHUDWidget>(nullptr, TEXT("/Game/UI/WBP_HealthHUD.WBP_HealthHUD_C"));
    if (!TestNotNull(TEXT("HUD class"), Class)) return false;
    TStrongObjectPtr<UCombatHUDWidget> HUD(NewObject<UCombatHUDWidget>(GetTransientPackage(), Class));
    if (!TestTrue(TEXT("initialize HUD"), HUD->Initialize())) return false;
    auto* Inv = InventoryTests::Inventory(InventoryTests::Table());
    Inv->TryAddItem(TEXT("Apple"), 15);
    HUD->BindInventory(Inv);
    Inv->AssignQuickSlot(0, 0);
    auto* Label = Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("WBP_QuickSlotLabel_1")));
    if (!TestNotNull(TEXT("label"), Label)) return false;
    TestEqual(TEXT("all stacks counted"), Label->GetText().ToString(), FText::AsNumber(15).ToString());
    Inv->RemoveFromSlot(0, 10);
    TestEqual(TEXT("remaining stack counted"), Label->GetText().ToString(), FText::AsNumber(5).ToString());
    Inv->RemoveItem(TEXT("Apple"), 5);
    TestTrue(TEXT("empty count hidden"), Label->GetVisibility() == ESlateVisibility::Collapsed);
    HUD->BindInventory(nullptr);
    return true;
}
#endif
