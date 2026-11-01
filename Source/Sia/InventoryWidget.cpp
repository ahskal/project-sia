#include "InventoryWidget.h"
#include "BasePlayerController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "InputCoreTypes.h"

void UInventorySlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SlotButton->OnClicked.AddDynamic(this, &UInventorySlotWidget::HandleClicked);
}

void UInventorySlotWidget::SetItem(int32 Index, const FInventoryItemView& Item)
{
    SlotIndex = Index;
    const bool bOccupied = !Item.ItemId.IsNone() && Item.Quantity > 0;
    SlotIndexText->SetText(FText::FromString(FString::Printf(TEXT("%02d"), Index + 1)));
    ItemIcon->SetBrushFromTexture(bOccupied ? Item.Icon.Get() : nullptr);
    ItemIcon->SetVisibility(bOccupied && Item.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    QuantityText->SetText(bOccupied ? FText::AsNumber(Item.Quantity) : FText::GetEmpty());
    SlotButton->SetToolTipText(bOccupied ? Item.DisplayName : FText::FromString(TEXT("Empty slot")));
}

void UInventorySlotWidget::SetSelected(bool bSelected)
{
    SlotBackground->SetBrushColor(bSelected ? FLinearColor(.06f, .19f, .15f, 1.f)
        : FLinearColor(.035f, .05f, .075f, 1.f));
}

void UInventorySlotWidget::HandleClicked()
{
    OnSlotClicked.Broadcast(SlotIndex);
}

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SlotWidgetClass = TSoftClassPtr<UInventorySlotWidget>(
        FSoftObjectPath(TEXT("/Game/UI/WBP_InventorySlot.WBP_InventorySlot_C")));
}

void UInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::CloseInventory);
    BuildSlots();
    SetItems({});
}

void UInventoryWidget::BuildSlots()
{
    // Slot capacity must not depend on children saved in the designer.
    // Empty inventories still need visible, selectable slots.
    UClass* LoadedClass = SlotWidgetClass.LoadSynchronous();
    if (!ItemGrid || !LoadedClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Inventory slot grid or WBP class is missing: %s"),
            *SlotWidgetClass.ToSoftObjectPath().ToString());
        return;
    }

    ItemGrid->ClearChildren();
    Slots.Reset();
    const int32 Capacity = FMath::Clamp(SlotCount, 1, 120);
    const int32 ColumnCount = FMath::Clamp(Columns, 1, Capacity);
    for (int32 Index = 0; Index < Capacity; ++Index)
    {
        auto* ItemSlot = CreateWidget<UInventorySlotWidget>(this, LoadedClass);
        if (!ItemSlot)
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to create inventory slot %d"), Index);
            break;
        }
        auto* GridSlot = ItemGrid->AddChildToUniformGrid(ItemSlot, Index / ColumnCount, Index % ColumnCount);
        GridSlot->SetHorizontalAlignment(HAlign_Fill);
        GridSlot->SetVerticalAlignment(VAlign_Fill);
        Slots.Add(ItemSlot);
        ItemSlot->OnSlotClicked.AddDynamic(this, &UInventoryWidget::HandleSlotClicked);
    }
}

void UInventoryWidget::SetItems(const TArray<FInventoryItemView>& Items)
{
    DisplayItems = Items;
    DisplayItems.SetNum(Slots.Num());
    int32 Occupied = 0;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        auto& Item = DisplayItems[Index];
        Item.Quantity = FMath::Max(0, Item.Quantity);
        if (!Item.ItemId.IsNone() && Item.Quantity > 0) ++Occupied;
        Slots[Index]->SetItem(Index, Item);
    }
    CapacityText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d SLOTS"), Occupied, Slots.Num())));
    RefreshSelection();
}

void UInventoryWidget::HandleSlotClicked(int32 Index)
{
    if (!DisplayItems.IsValidIndex(Index)) return;
    SelectedIndex = Index;
    RefreshSelection();
    OnItemSelected.Broadcast(Index, DisplayItems[Index].ItemId);
}

void UInventoryWidget::RefreshSelection()
{
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
        Slots[Index]->SetSelected(Index == SelectedIndex);
    const FInventoryItemView* Item = DisplayItems.IsValidIndex(SelectedIndex)
        ? &DisplayItems[SelectedIndex] : nullptr;
    const bool bOccupied = Item && !Item->ItemId.IsNone() && Item->Quantity > 0;
    SelectionName->SetText(bOccupied ? Item->DisplayName
        : FText::FromString(Item ? TEXT("EMPTY SLOT") : TEXT("SELECT AN ITEM")));
    SelectionDescription->SetText(bOccupied ? Item->Description
        : FText::FromString(TEXT("Select a slot to view its details.")));
    SelectionQuantity->SetText(bOccupied
        ? FText::Format(FText::FromString(TEXT("QUANTITY  {0}")), FText::AsNumber(Item->Quantity))
        : FText::GetEmpty());
}

void UInventoryWidget::CloseInventory()
{
    if (auto* PC = Cast<ABasePlayerController>(GetOwningPlayer())) PC->ToggleInventory();
}

FReply UInventoryWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::I || KeyEvent.GetKey() == EKeys::Escape)
    {
        CloseInventory();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}
