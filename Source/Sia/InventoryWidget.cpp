#include "InventoryWidget.h"
#include "BasePlayerController.h"
#include "CombatHUDWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/SpinBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "InputCoreTypes.h"
#include "BaseCharacter.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InventoryComponent.h"
#include "Items/ItemBase.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
UTexture2D* CreateEquipmentFallbackIcon()
{
    constexpr int32 Size = 64;
    UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
    if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.IsEmpty()) return nullptr;
    FColor* Pixels = static_cast<FColor*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
    if (!Pixels) return nullptr;
    for (int32 Y = 0; Y < Size; ++Y)
    {
        for (int32 X = 0; X < Size; ++X)
        {
            const bool bFrame = X < 4 || Y < 4 || X >= Size - 4 || Y >= Size - 4;
            const bool bCross = FMath::Abs(X - Y) <= 4 || FMath::Abs((Size - 1 - X) - Y) <= 4;
            Pixels[Y * Size + X] = bFrame ? FColor(220, 220, 220, 255)
                : bCross ? FColor(230, 70, 70, 255) : FColor(45, 45, 45, 255);
        }
    }
    Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
    Texture->SRGB = true;
    Texture->UpdateResource();
    return Texture;
}
}

UInventorySlotWidget::UInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<UUserWidget> TooltipClass(TEXT("/Game/UI/WBP_ItemTooltip"));
    ItemTooltipClass = TooltipClass.Class;
    SelectionBrush = FSlateRoundedBoxBrush(FLinearColor(.055f,.036f,.014f), 5.f,
        FLinearColor(.95f,.60f,.19f), 2.f);
}

void UInventorySlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    NormalBrush = SlotBackground->Background;
    SlotButton->OnClicked.AddDynamic(this, &UInventorySlotWidget::HandleClicked);
}

void UInventorySlotWidget::SetItem(int32 Index, const FInventoryItemView& Item)
{
    SlotIndex = Index;
    const bool bOccupied = !Item.ItemId.IsNone() && Item.Quantity > 0;
    SlotIndexText->SetText(bOccupied ? FText::FromString(Item.DisplayName.ToString().Left(2)) : FText::GetEmpty());
    SlotIndexText->SetVisibility(bOccupied && !Item.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    ItemIcon->SetBrushFromTexture(bOccupied ? Item.Icon.Get() : nullptr);
    ItemIcon->SetRenderOpacity(bOccupied && Item.Icon ? 1.f : 0.f);
    ItemIcon->SetVisibility(bOccupied && Item.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    QuantityText->SetText(bOccupied ? FText::AsNumber(Item.Quantity) : FText::GetEmpty());
    SlotButton->SetToolTipText(FText::GetEmpty());
    if (!bOccupied)
    {
        SlotButton->SetToolTip(nullptr);
        return;
    }

    auto* Tooltip = Cast<UUserWidget>(SlotButton->GetToolTip());
    if (!Tooltip && ItemTooltipClass)
        Tooltip = CreateWidget<UUserWidget>(this, ItemTooltipClass);
    if (!Tooltip) return;

    // 레이아웃과 스타일은 WBP에서, 실제 표시 값은 인벤토리에서 관리한다.
    const UItemBase* SourceItem = DragInventory
        ? (bEquipmentSlot ? DragInventory->GetEquippedItem(EquipmentSlot) : DragInventory->GetItemAt(Index))
        : nullptr;
    const FItemData* Data = SourceItem ? &SourceItem->GetData() : nullptr;
    const auto SetText = [Tooltip](const TCHAR* Name, const FText& Text)
    {
        if (auto* Block = Cast<UTextBlock>(Tooltip->GetWidgetFromName(FName(Name))))
        {
            Block->SetText(Text);
            Block->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
        }
    };
    SetText(TEXT("TooltipName"), Item.DisplayName);
    SetText(TEXT("TooltipDescription"), Item.Description);
    SetText(TEXT("TooltipQuantity"), FText::Format(
        NSLOCTEXT("Inventory", "TooltipQuantity", "보유 수량: {0}"), FText::AsNumber(Item.Quantity)));
    SetText(TEXT("TooltipCategory"), Data ? FText::Format(
        NSLOCTEXT("Inventory", "TooltipCategory", "{0} · {1}"),
        StaticEnum<EItemQuality>()->GetDisplayNameTextByValue(int64(Data->ItemQuality)),
        StaticEnum<EItemType>()->GetDisplayNameTextByValue(int64(Data->ItemType))) : FText::GetEmpty());
    SetText(TEXT("TooltipEffect"), Data && Data->ItemStatistics.RestorationAmount > 0.f
        ? FText::Format(NSLOCTEXT("Inventory", "TooltipHealing", "♥  체력 회복 +{0}"),
            FText::AsNumber(Data->ItemStatistics.RestorationAmount)) : FText::GetEmpty());
    FText Usage;
    if (Data)
    {
        if (bEquipmentSlot) Usage = NSLOCTEXT("Inventory", "TooltipUnequip", "우클릭: 장착 해제");
        else if (Data->ItemType == EItemType::Consumable) Usage = NSLOCTEXT("Inventory", "TooltipUse", "우클릭: 사용");
        else if (Data->EquipmentSlot != EEquipmentSlot::None) Usage = NSLOCTEXT("Inventory", "TooltipEquip", "우클릭: 장착");
        else Usage = Data->TextData.UsageText;
    }
    SetText(TEXT("TooltipUsage"), Usage);
    if (auto* Icon = Cast<UImage>(Tooltip->GetWidgetFromName(TEXT("TooltipIcon"))))
    {
        Icon->SetBrushFromTexture(Item.Icon.Get());
        Icon->SetVisibility(Item.Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    SlotButton->SetToolTip(Tooltip);
}

void UInventorySlotWidget::SetEquipmentItem(EEquipmentSlot InEquipmentSlot, UInventoryComponent* Inventory,
    UTexture2D* FallbackIcon, const FText& EmptyLabel)
{
    bEquipmentSlot = true;
    EquipmentSlot = InEquipmentSlot;
    DragInventory = Inventory;
    SlotIndex = INDEX_NONE;

    UItemBase* Item = Inventory ? Inventory->GetEquippedItem(InEquipmentSlot) : nullptr;
    if (!Item)
    {
        ItemIcon->SetBrushFromTexture(nullptr);
        ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        ItemIcon->SetRenderOpacity(0.f);
        SlotIndexText->SetText(EmptyLabel);
        SlotIndexText->SetVisibility(ESlateVisibility::HitTestInvisible);
        QuantityText->SetText(FText::GetEmpty());
        QuantityText->SetVisibility(ESlateVisibility::Collapsed);
        SlotButton->SetToolTipText(FText::GetEmpty());
        SlotButton->SetToolTip(nullptr);
        return;
    }

    const FItemData& Data = Item->GetData();
    FInventoryItemView View;
    View.ItemId = Item->GetItemId();
    View.DisplayName = Data.TextData.DisplayName;
    View.Description = Data.TextData.Description;
    View.Icon = Data.AssetData.Icon.LoadSynchronous();
    if (!View.Icon) View.Icon = FallbackIcon;
    View.Quantity = 1;
    View.UnitWeight = Data.NumericData.Weight;
    SetItem(INDEX_NONE, View);
    SlotIndexText->SetText(FText::GetEmpty());
    SlotIndexText->SetVisibility(ESlateVisibility::Collapsed);
    QuantityText->SetText(FText::GetEmpty());
    QuantityText->SetVisibility(ESlateVisibility::Collapsed);
}

void UInventorySlotWidget::SetSelected(bool bSelected)
{
    SlotBackground->SetBrush(bSelected ? SelectionBrush : NormalBrush);
}

void UInventorySlotWidget::HandleClicked()
{
    if (bEquipmentSlot) return;
    OnSlotClicked.Broadcast(SlotIndex);
}

FReply UInventorySlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (!bEquipmentSlot && SlotIndex != INDEX_NONE)
            OnSlotContextRequested.Broadcast(SlotIndex);
        return FReply::Handled();
    }
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        return UWidgetBlueprintLibrary::DetectDragIfPressed(MouseEvent, this, EKeys::LeftMouseButton).NativeReply;
    return Super::NativeOnPreviewMouseButtonDown(Geometry, MouseEvent);
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry&, const FDragDropEvent&, UDragDropOperation* Operation)
{
    auto* Drag = Cast<UInventoryItemDragOperation>(Operation);
    if (!Drag || !DragInventory || Drag->Inventory != DragInventory) return false;

    if (bEquipmentSlot)
    {
        FText Message;
        bool bSuccess = false;
        if (Drag->bFromEquipment)
        {
            if (Drag->EquipmentSlot == EquipmentSlot) return true;
            Message = FText::FromString(TEXT("장비를 같은 종류의 장비 슬롯으로만 옮길 수 있습니다."));
        }
        else
        {
            bSuccess = DragInventory->EquipItemToSlot(Drag->SlotIndex, EquipmentSlot, Message);
        }
        if (UInventoryWidget* InventoryWidget = GetTypedOuter<UInventoryWidget>())
            InventoryWidget->ShowInventoryMessage(Message);
        else if (!bSuccess)
            SetToolTipText(Message);
        return true;
    }

    if (Drag->bFromEquipment)
    {
        FText Message;
        const bool bUnequipped = DragInventory->UnequipItemToSlot(Drag->EquipmentSlot, SlotIndex, Message);
        if (UInventoryWidget* InventoryWidget = GetTypedOuter<UInventoryWidget>())
            InventoryWidget->ShowInventoryMessage(Message);
        else if (!bUnequipped)
            SetToolTipText(Message);
        return true;
    }

    // Dropping onto another inventory cell moves, swaps, or merges the stack.
    // A rejected move is still consumed so it cannot accidentally drop the item into the world.
    if (SlotIndex != Drag->SlotIndex)
        DragInventory->MoveOrMerge(Drag->SlotIndex, SlotIndex);
    return true;
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry&, const FPointerEvent&, UDragDropOperation*& Operation)
{
    if (!DragInventory) return;
    auto* Drag = NewObject<UInventoryItemDragOperation>(this);
    Drag->Inventory = DragInventory;
    if (bEquipmentSlot)
    {
        if (!DragInventory->GetEquippedItem(EquipmentSlot)) return;
        Drag->bFromEquipment = true;
        Drag->EquipmentSlot = EquipmentSlot;
    }
    else
    {
        if (SlotIndex == INDEX_NONE || !DragInventory->GetItemAt(SlotIndex)) return;
        Drag->SlotIndex = SlotIndex;
    }
    Drag->DefaultDragVisual = this;
    Drag->Pivot = EDragPivot::MouseDown;
    Operation = Drag;
}

void UInventorySlotWidget::NativeOnDragCancelled(const FDragDropEvent& Event, UDragDropOperation* Operation)
{
    Super::NativeOnDragCancelled(Event, Operation);
    // Escape/programmatic cancellation has an empty pointer event.
    // Only an actual left-button release may drop items into the world.
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton
        || Event.IsMouseButtonDown(EKeys::LeftMouseButton)) return;
    auto* Drag = Cast<UInventoryItemDragOperation>(Operation);
    if (!Drag || !Drag->Inventory) return;
    if (Drag->bFromEquipment)
    {
        Drag->Inventory->DropEquippedItem(Drag->EquipmentSlot);
        return;
    }
    UItemBase* Item = Drag->Inventory->GetItemAt(Drag->SlotIndex);
    if (!Item) return;
    if (Item->GetQuantity() > 1)
    {
        if (UInventoryWidget* InventoryWidget = GetTypedOuter<UInventoryWidget>())
        {
            InventoryWidget->RequestDropItemForSlot(Drag->SlotIndex);
            return;
        }
    }
    // 슬롯이 다른 위젯에서 생성되어 수량 선택 창을 찾지 못해도
    // 드래그 취소가 아이템 버리기로 동작하도록 한 개를 월드에 둔다.
    Drag->Inventory->DropItemFromSlot(Drag->SlotIndex, 1);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (bEquipmentSlot) return FReply::Handled();
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        OnSlotEquipRequested.Broadcast(SlotIndex);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonDoubleClick(Geometry, MouseEvent);
}

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<UUserWidget> ContextMenuWidget(TEXT("/Game/UI/WBP_InventoryItemContextMenu"));
    ItemContextMenuClass = ContextMenuWidget.Class;
    SlotWidgetClass = TSoftClassPtr<UInventorySlotWidget>(
        FSoftObjectPath(TEXT("/Game/UI/WBP_InventorySlot.WBP_InventorySlot_C")));
}

void UInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(true);
    Button_InventoryTab->OnClicked.AddDynamic(this, &UInventoryWidget::FocusInventory);
    Button_EquipmentTab->OnClicked.AddDynamic(this, &UInventoryWidget::FocusEquipment);
    CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::CloseInventory);
    WBP_DropConfirm->OnClicked.AddDynamic(this, &UInventoryWidget::ConfirmDropItem);
    WBP_DropCancel->OnClicked.AddDynamic(this, &UInventoryWidget::CancelDropItem);
    InventoryRoot = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (InventoryRoot && ItemContextMenuClass)
    {
        ItemContextMenu = CreateWidget<UUserWidget>(this, ItemContextMenuClass);
        ContextUseButton = Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("ContextUse")));
        ContextShortcutButton = Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("ContextShortcut")));
        ContextDropButton = Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("ContextDrop")));
        ContextBackButton = Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("ContextBack")));
        ContextQuickSlotButtons = {
            Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("QuickSlot1"))),
            Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("QuickSlot2"))),
            Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("QuickSlot3"))),
            Cast<UButton>(ItemContextMenu->GetWidgetFromName(TEXT("QuickSlot4"))) };
        if (ContextUseButton) ContextUseButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleContextUse);
        if (ContextShortcutButton) ContextShortcutButton->OnClicked.AddDynamic(this, &UInventoryWidget::ShowContextQuickSlots);
        if (ContextBackButton) ContextBackButton->OnClicked.AddDynamic(this, &UInventoryWidget::ShowContextOptions);
        if (ContextDropButton) ContextDropButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleContextDrop);
        for (int32 Index = 0; Index < 4; ++Index)
        {
            UButton* Button = ContextQuickSlotButtons[Index];
            if (!Button) continue;
            if (Index == 0) Button->OnClicked.AddDynamic(this, &UInventoryWidget::AssignContextQuickSlot1);
            else if (Index == 1) Button->OnClicked.AddDynamic(this, &UInventoryWidget::AssignContextQuickSlot2);
            else if (Index == 2) Button->OnClicked.AddDynamic(this, &UInventoryWidget::AssignContextQuickSlot3);
            else Button->OnClicked.AddDynamic(this, &UInventoryWidget::AssignContextQuickSlot4);
        }
        InventoryRoot->AddChildToCanvas(ItemContextMenu);
        if (UCanvasPanelSlot* MenuCanvasSlot = Cast<UCanvasPanelSlot>(ItemContextMenu->Slot))
        {
            MenuCanvasSlot->SetAutoSize(true);
            MenuCanvasSlot->SetZOrder(100);
        }
        ItemContextMenu->SetVisibility(ESlateVisibility::Collapsed);
    }
    BuildSlots();
    SetItems({});
}

void UInventoryWidget::RequestDropItemForSlot(int32 InventorySlot)
{
    UItemBase* Item = BoundInventory ? BoundInventory->GetItemAt(InventorySlot) : nullptr;
    if (!Item || Item->GetQuantity() <= 1) return;
    if (!WBP_DropBackdrop || !WBP_DropCount || !WBP_DropName)
    {
        const bool bDropped = BoundInventory->DropItemFromSlot(InventorySlot, 1);
        ShowInventoryMessage(bDropped
            ? FText::FromString(TEXT("아이템 1개를 버렸습니다."))
            : FText::FromString(TEXT("아이템을 버리지 못했습니다.")));
        return;
    }
    PendingDropSlot = InventorySlot;
    PendingDropItem = Item;
    WBP_DropName->SetText(FText::Format(FText::FromString(TEXT("{0} · 보유 {1}개")),
        Item->GetData().TextData.DisplayName, FText::AsNumber(Item->GetQuantity())));
    WBP_DropCount->SetMinValue(1.f);
    WBP_DropCount->SetMaxValue(static_cast<float>(Item->GetQuantity()));
    WBP_DropCount->SetMinSliderValue(1.f);
    WBP_DropCount->SetMaxSliderValue(static_cast<float>(Item->GetQuantity()));
    WBP_DropCount->SetValue(1.f);
    WBP_DropBackdrop->SetVisibility(ESlateVisibility::Visible);
    WBP_DropCount->SetKeyboardFocus();
}

void UInventoryWidget::ShowInventoryMessage(const FText& Message)
{
    if (InventoryHint) InventoryHint->SetText(Message);
}

void UInventoryWidget::ConfirmDropItem()
{
    UItemBase* Item = BoundInventory ? BoundInventory->GetItemAt(PendingDropSlot) : nullptr;
    if (!Item || Item != PendingDropItem.Get() || Item->GetQuantity() <= 0 || !WBP_DropCount)
    {
        CloseDropDialog();
        return;
    }
    const int32 Quantity = FMath::Clamp(FMath::RoundToInt(WBP_DropCount->GetValue()), 1, Item->GetQuantity());
    if (!BoundInventory->DropItemFromSlot(PendingDropSlot, Quantity))
    {
        if (InventoryHint) InventoryHint->SetText(FText::FromString(TEXT("아이템을 내려놓지 못했습니다.")));
        return;
    }
    CloseDropDialog();
}

void UInventoryWidget::CancelDropItem()
{
    CloseDropDialog();
}

void UInventoryWidget::CloseDropDialog()
{
    PendingDropSlot = INDEX_NONE;
    PendingDropItem.Reset();
    if (WBP_DropBackdrop) WBP_DropBackdrop->SetVisibility(ESlateVisibility::Collapsed);
}

void UInventoryWidget::BuildSlots()
{
    // Reuse WBP designer slots; create slots only when the configured capacity changes.
    UClass* LoadedClass = SlotWidgetClass.LoadSynchronous();
    if (!ItemGrid || !LoadedClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Inventory slot grid or WBP class is missing: %s"),
            *SlotWidgetClass.ToSoftObjectPath().ToString());
        return;
    }

    Slots.Reset();
    const int32 Capacity = FMath::Clamp(SlotCount, 1, 120);
    const int32 ColumnCount = FMath::Clamp(Columns, 1, Capacity);
    bool bUseDesignerSlots = ItemGrid->GetChildrenCount() == Capacity;
    for (int32 Index = 0; bUseDesignerSlots && Index < Capacity; ++Index)
        bUseDesignerSlots = Cast<UInventorySlotWidget>(ItemGrid->GetChildAt(Index)) != nullptr;
    if (bUseDesignerSlots)
    {
        for (int32 Index = 0; Index < Capacity; ++Index)
        {
            auto* ItemSlot = CastChecked<UInventorySlotWidget>(ItemGrid->GetChildAt(Index));
            ItemSlot->SetDragInventory(BoundInventory);
            Slots.Add(ItemSlot);
            ItemSlot->OnSlotClicked.AddUniqueDynamic(this, &UInventoryWidget::HandleSlotClicked);
            ItemSlot->OnSlotUseRequested.AddUniqueDynamic(this, &UInventoryWidget::HandleSlotUseRequested);
            ItemSlot->OnSlotEquipRequested.AddUniqueDynamic(this, &UInventoryWidget::HandleSlotEquipRequested);
            ItemSlot->OnSlotContextRequested.AddUniqueDynamic(this, &UInventoryWidget::HandleSlotContextRequested);
        }
        return;
    }
    ItemGrid->ClearChildren();
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
        ItemSlot->SetDragInventory(BoundInventory);
        ItemSlot->OnSlotClicked.AddDynamic(this, &UInventoryWidget::HandleSlotClicked);
        ItemSlot->OnSlotUseRequested.AddDynamic(this, &UInventoryWidget::HandleSlotUseRequested);
        ItemSlot->OnSlotEquipRequested.AddDynamic(this, &UInventoryWidget::HandleSlotEquipRequested);
        ItemSlot->OnSlotContextRequested.AddDynamic(this, &UInventoryWidget::HandleSlotContextRequested);
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
    CapacityText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Occupied, Slots.Num())));
    float Weight = 0.f;
    for (const auto& Item : DisplayItems)
        if (!Item.ItemId.IsNone()) Weight += FMath::Max(0.f, Item.UnitWeight) * Item.Quantity;
    Text_WeightValue->SetText(FText::FromString(FString::Printf(TEXT("보유 무게   %.1f / %.1f"), Weight, DisplayMaxWeight)));
    RefreshSelection();
}

void UInventoryWidget::BindInventory(UInventoryComponent* Inventory)
{
    CloseDropDialog();
    CloseItemContextMenu();
    if (BoundInventory) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::RefreshFromInventory);
    BoundInventory = Inventory;
    for (const TObjectPtr<UInventorySlotWidget>& ItemSlot : Slots) ItemSlot->SetDragInventory(BoundInventory);
    if (BoundInventory)
    {
        BoundInventory->OnInventoryChanged.AddUniqueDynamic(this, &UInventoryWidget::RefreshFromInventory);
        if (Slots.Num() != BoundInventory->GetMaxSlots())
        {
            SlotCount = BoundInventory->GetMaxSlots();
            BuildSlots();
        }
    }
    RefreshFromInventory();
}

bool UInventoryWidget::NativeOnDrop(const FGeometry&, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation)
{
    auto* Drag = Cast<UInventoryItemDragOperation>(Operation);
    if (!Drag || !BoundInventory || Drag->Inventory != BoundInventory) return false;
    if (Drag->bFromEquipment) return false;

    const FVector2D DropPosition = DragDropEvent.GetScreenSpacePosition();
    if (auto* PC = Cast<ABasePlayerController>(GetOwningPlayer()))
    {
        if (UCombatHUDWidget* HUD = PC->GetCombatHUDWidget())
        {
            const bool bAssigned = HUD->TryAssignQuickSlotAtPosition(
                DropPosition, Drag->Inventory, Drag->SlotIndex);
            if (bAssigned && InventoryHint)
                InventoryHint->SetText(FText::FromString(TEXT("퀵슬롯에 등록했습니다.")));
            if (bAssigned) return true;
        }
    }
    return false;
}

void UInventoryWidget::RefreshFromInventory()
{
    TArray<FInventoryItemView> Views;
    DisplayMaxWeight = BoundInventory ? BoundInventory->GetMaxWeight() : 50.f;
    if (BoundInventory)
    {
        Views.SetNum(BoundInventory->GetMaxSlots());
        for (int32 Index = 0; Index < Views.Num(); ++Index)
        {
            UItemBase* Item = BoundInventory->GetItemAt(Index);
            if (!Item) continue;
            const auto& Data = Item->GetData();
            auto& View = Views[Index];
            View.ItemId = Item->GetItemId();
            View.DisplayName = Data.TextData.DisplayName;
            View.Description = Data.TextData.Description;
            View.Quantity = Item->GetQuantity();
            View.UnitWeight = Data.NumericData.Weight;
            View.Icon = Data.AssetData.Icon.LoadSynchronous();
        }
    }
    SetItems(Views);
    RefreshEquipmentDisplay();
}

void UInventoryWidget::HandleSlotUseRequested(int32 Index)
{
    if (!BoundInventory) return;
    FText Message;
    BoundInventory->UseItem(Index, Message);
    if (InventoryHint) InventoryHint->SetText(Message);
}

void UInventoryWidget::HandleSlotEquipRequested(int32 Index)
{
    if (!BoundInventory) return;
    FText Message;
    const UItemBase* Item = BoundInventory->GetItemAt(Index);
    if (Item && Item->GetData().ItemType == EItemType::Consumable)
        BoundInventory->UseItem(Index, Message);
    else
        BoundInventory->EquipItem(Index, Message);
    if (InventoryHint) InventoryHint->SetText(Message);
}

void UInventoryWidget::HandleSlotContextRequested(int32 Index)
{
    if (!ItemContextMenu || !InventoryRoot) return;
    if (!BoundInventory || !BoundInventory->GetItemAt(Index))
    {
        ItemContextMenu->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    ContextItemIndex = Index;
    ShowContextOptions();
    PositionContextMenu(112.f);
    ItemContextMenu->SetVisibility(ESlateVisibility::Visible);
    ContextUseButton->SetKeyboardFocus();
}

void UInventoryWidget::HandleContextUse()
{
    ItemContextMenu->SetVisibility(ESlateVisibility::Collapsed);
    const UItemBase* Item = BoundInventory ? BoundInventory->GetItemAt(ContextItemIndex) : nullptr;
    if (!Item) return;
    if (Item->GetData().ItemType == EItemType::Consumable)
        HandleSlotUseRequested(ContextItemIndex);
    else if (Item->GetData().ItemType == EItemType::Armor || Item->GetData().ItemType == EItemType::Weapon)
        HandleSlotEquipRequested(ContextItemIndex);
    else
        ShowInventoryMessage(FText::FromString(TEXT("이 아이템은 사용할 수 없습니다.")));
}

void UInventoryWidget::HandleContextDrop()
{
    if (ItemContextMenu) ItemContextMenu->SetVisibility(ESlateVisibility::Collapsed);
    const UItemBase* Item = BoundInventory ? BoundInventory->GetItemAt(ContextItemIndex) : nullptr;
    if (!Item) return;
    if (Item->GetQuantity() > 1)
        RequestDropItemForSlot(ContextItemIndex);
    else if (BoundInventory->DropItemFromSlot(ContextItemIndex, 1))
        ShowInventoryMessage(FText::FromString(TEXT("아이템 1개를 버렸습니다.")));
    else
        ShowInventoryMessage(FText::FromString(TEXT("아이템을 버리지 못했습니다.")));
}

void UInventoryWidget::ShowContextQuickSlots()
{
    if (!ItemContextMenu || ContextItemIndex == INDEX_NONE) return;
    if (ContextUseButton) ContextUseButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextShortcutButton) ContextShortcutButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextDropButton) ContextDropButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextBackButton) ContextBackButton->SetVisibility(ESlateVisibility::Visible);
    for (UButton* Button : ContextQuickSlotButtons)
        if (Button) Button->SetVisibility(ESlateVisibility::Visible);
    PositionContextMenu(176.f);
}

void UInventoryWidget::ShowContextOptions()
{
    if (!ItemContextMenu || ContextItemIndex == INDEX_NONE) return;
    for (UButton* Button : ContextQuickSlotButtons)
        if (Button) Button->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextBackButton) ContextBackButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextUseButton) ContextUseButton->SetVisibility(ESlateVisibility::Visible);
    if (ContextShortcutButton) ContextShortcutButton->SetVisibility(ESlateVisibility::Visible);
    if (ContextDropButton) ContextDropButton->SetVisibility(ESlateVisibility::Visible);
    PositionContextMenu(112.f);
}

void UInventoryWidget::CloseItemContextMenu()
{
    CloseDropDialog();
    ContextItemIndex = INDEX_NONE;
    for (UButton* Button : ContextQuickSlotButtons)
        if (Button) Button->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextBackButton) ContextBackButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ContextUseButton) ContextUseButton->SetVisibility(ESlateVisibility::Visible);
    if (ContextShortcutButton) ContextShortcutButton->SetVisibility(ESlateVisibility::Visible);
    if (ContextDropButton) ContextDropButton->SetVisibility(ESlateVisibility::Visible);
    if (ItemContextMenu) ItemContextMenu->SetVisibility(ESlateVisibility::Collapsed);
}

void UInventoryWidget::PositionContextMenu(float MenuHeight)
{
    if (!InventoryRoot || !ItemContextMenu || !Slots.IsValidIndex(ContextItemIndex) || !Slots[ContextItemIndex]) return;
    const FGeometry& ItemGeometry = Slots[ContextItemIndex]->GetCachedGeometry();
    const FVector2D ItemAbsolute = ItemGeometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D ItemSize = ItemGeometry.GetLocalSize();
    const FGeometry& RootGeometry = InventoryRoot->GetCachedGeometry();
    const FVector2D ItemTopLeft = RootGeometry.AbsoluteToLocal(ItemAbsolute);
    const FVector2D ItemTopRight = RootGeometry.AbsoluteToLocal(
        ItemGeometry.LocalToAbsolute(FVector2D(ItemSize.X, 0.f)));
    const FVector2D RootSize = RootGeometry.GetLocalSize();
    const float DesiredMenuWidth = ItemContextMenu->GetDesiredSize().X;
    const float MenuWidth = DesiredMenuWidth > 0.f ? DesiredMenuWidth : 120.f;
    constexpr float MenuGap = 2.f;

    FVector2D Position(ItemTopRight.X + MenuGap, ItemTopLeft.Y + (ItemSize.Y - MenuHeight) * .5f);
    if (Position.X + MenuWidth > RootSize.X)
        Position.X = ItemTopLeft.X - MenuWidth - MenuGap;
    Position.X = FMath::Clamp(Position.X, 0.f, FMath::Max(0.f, RootSize.X - MenuWidth));
    Position.Y = FMath::Clamp(Position.Y, 0.f, FMath::Max(0.f, RootSize.Y - MenuHeight));
    if (UCanvasPanelSlot* MenuCanvasSlot = Cast<UCanvasPanelSlot>(ItemContextMenu->Slot))
        MenuCanvasSlot->SetPosition(Position);
}

void UInventoryWidget::AssignContextQuickSlot1() { AssignContextQuickSlot(0); }
void UInventoryWidget::AssignContextQuickSlot2() { AssignContextQuickSlot(1); }
void UInventoryWidget::AssignContextQuickSlot3() { AssignContextQuickSlot(2); }
void UInventoryWidget::AssignContextQuickSlot4() { AssignContextQuickSlot(3); }

void UInventoryWidget::AssignContextQuickSlot(int32 QuickSlotIndex)
{
    if (ItemContextMenu) ItemContextMenu->SetVisibility(ESlateVisibility::Collapsed);
    if (!BoundInventory || !BoundInventory->GetItemAt(ContextItemIndex)) return;
    const bool bAssigned = BoundInventory->AssignQuickSlot(QuickSlotIndex, ContextItemIndex);
    ShowInventoryMessage(bAssigned
        ? FText::Format(FText::FromString(TEXT("퀵슬롯 {0}에 등록했습니다.")), FText::AsNumber(QuickSlotIndex + 1))
        : FText::FromString(TEXT("퀵슬롯에 등록하지 못했습니다.")));
}

void UInventoryWidget::RefreshEquipmentDisplay()
{
    if (!BoundInventory) return;
    static const EEquipmentSlot EquipmentSlots[] = {
        EEquipmentSlot::Head, EEquipmentSlot::Chest, EEquipmentSlot::Hands, EEquipmentSlot::Legs,
        EEquipmentSlot::Feet, EEquipmentSlot::Neck, EEquipmentSlot::Accessory, EEquipmentSlot::Ring,
        EEquipmentSlot::Weapon
    };
    UInventorySlotWidget* EquipmentWidgets[] = {
        EquipmentItemSlot_Head, EquipmentItemSlot_Chest, EquipmentItemSlot_Hands, EquipmentItemSlot_Legs,
        EquipmentItemSlot_Feet, EquipmentItemSlot_Neck, EquipmentItemSlot_Accessory, EquipmentItemSlot_Ring,
        EquipmentItemSlot_Weapon
    };
    static const TCHAR* EmptyLabels[] = {
        TEXT("머리"), TEXT("갑옷"), TEXT("장갑"), TEXT("바지"), TEXT("신발"),
        TEXT("목걸이"), TEXT("장신구"), TEXT("반지"), TEXT("무기")
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(EquipmentSlots); ++Index)
    {
        if (!EquipmentWidgets[Index]) continue;
        if (!FallbackEquipmentIcon) FallbackEquipmentIcon = CreateEquipmentFallbackIcon();
        EquipmentWidgets[Index]->SetEquipmentItem(EquipmentSlots[Index], BoundInventory,
            FallbackEquipmentIcon, FText::FromString(EmptyLabels[Index]));
    }
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
    if (SelectionName) SelectionName->SetText(bOccupied ? Item->DisplayName
        : FText::FromString(Item ? TEXT("EMPTY SLOT") : TEXT("SELECT AN ITEM")));
    if (SelectionDescription) SelectionDescription->SetText(bOccupied ? Item->Description
        : FText::FromString(TEXT("Select a slot to view its details.")));
    if (SelectionQuantity) SelectionQuantity->SetText(bOccupied
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
        if (KeyEvent.GetKey() == EKeys::Escape && WBP_DropBackdrop
            && WBP_DropBackdrop->GetVisibility() != ESlateVisibility::Collapsed)
        {
            CancelDropItem();
            return FReply::Handled();
        }
        CloseInventory();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

void UInventoryWidget::FocusInventory()
{
    Button_InventoryTab->SetBackgroundColor(FLinearColor(1.f,.77f,.40f));
    Button_EquipmentTab->SetBackgroundColor(FLinearColor(.5f,.5f,.5f));
    if (ItemGrid && ItemGrid->GetChildrenCount()) ItemGrid->GetChildAt(0)->SetKeyboardFocus();
}

void UInventoryWidget::FocusEquipment()
{
    Button_InventoryTab->SetBackgroundColor(FLinearColor(.5f,.5f,.5f));
    Button_EquipmentTab->SetBackgroundColor(FLinearColor(1.f,.77f,.40f));
    Button_EquipmentTab->SetKeyboardFocus();
}

void UInventoryWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!BoundInventory)
        if (auto* Character = Cast<ABaseCharacter>(GetOwningPlayerPawn())) BindInventory(Character->GetInventory());
    Image_CharacterPreview->SetColorAndOpacity(FLinearColor::White);
    if (!IsValid(PreviewActor) && GetWorld())
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        PreviewActor = GetWorld()->SpawnActor<ASceneCapture2D>(Params);
        if (PreviewActor)
        {
            PreviewTarget = NewObject<UTextureRenderTarget2D>(this);
            PreviewTarget->ClearColor = FLinearColor(.014f,.013f,.010f,1.f);
            PreviewTarget->InitAutoFormat(512, 512);
            auto* Capture = PreviewActor->GetCaptureComponent2D();
            Capture->TextureTarget = PreviewTarget;
            Capture->bCaptureEveryFrame = false;
            Capture->bCaptureOnMovement = false;
            Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
            Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
            Capture->FOVAngle = 35.f;
            Capture->ShowFlags.SetAtmosphere(false);
            Capture->ShowFlags.SetFog(false);
            Capture->ShowFlags.SetMotionBlur(false);
            Capture->ShowFlags.SetBloom(false);
            Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
            Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
            Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
            Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
            Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
            Capture->PostProcessSettings.AutoExposureBias = 0.f;
            // A separate lit stage avoids level shadows and never lights the gameplay character.
            if (auto* Character = Cast<ABaseCharacter>(GetOwningPlayerPawn()))
            {
                PreviewCharacterActor = GetWorld()->SpawnActor<AActor>(Params);
                PreviewMesh = NewObject<USkeletalMeshComponent>(PreviewCharacterActor);
                PreviewCharacterActor->SetRootComponent(PreviewMesh);
                PreviewMesh->SetSkeletalMeshAsset(Character->GetMesh()->GetSkeletalMeshAsset());
                PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                PreviewMesh->SetLightingChannels(false, true, false);
                PreviewMesh->SetCastShadow(false);
                PreviewMesh->RegisterComponent();
                PreviewMesh->SetWorldLocation(FVector(0, 0, -100000));
                PreviewMesh->SetWorldRotation(Character->GetMesh()->GetRelativeRotation());
                PreviewMesh->SetWorldScale3D(Character->GetMesh()->GetComponentScale());
                PreviewMesh->SetLeaderPoseComponent(Character->GetMesh());
                for (int32 Index = 0; Index < Character->GetMesh()->GetNumMaterials(); ++Index)
                    PreviewMesh->SetMaterial(Index, Character->GetMesh()->GetMaterial(Index));
                for (int32 Index = 0; Index < 2; ++Index)
                {
                    auto* Light = NewObject<UPointLightComponent>(PreviewCharacterActor);
                    Light->SetupAttachment(PreviewMesh);
                    Light->SetLightingChannels(false, true, false);
                    Light->SetIntensity(Index == 0 ? 60000.f : 35000.f);
                    Light->SetAttenuationRadius(1200.f);
                    Light->SetCastShadows(false);
                    Light->RegisterComponent();
                    Light->SetWorldLocation(FVector(250, Index == 0 ? -180.f : 180.f, -99820));
                }
                Capture->ShowOnlyActorComponents(PreviewCharacterActor);
            }
            FSlateBrush Brush;
            Brush.SetResourceObject(PreviewTarget);
            Brush.ImageSize = FVector2D(512, 512);
            Image_CharacterPreview->SetBrush(Brush);
        }
    }
    UpdateCharacterPreview();
}

void UInventoryWidget::NativeDestruct()
{
    CloseDropDialog();
    if (BoundInventory) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UInventoryWidget::RefreshFromInventory);
    BoundInventory = nullptr;
    if (IsValid(PreviewActor)) PreviewActor->Destroy();
    if (IsValid(PreviewCharacterActor)) PreviewCharacterActor->Destroy();
    PreviewCharacterActor = nullptr;
    PreviewMesh = nullptr;
    PreviewActor = nullptr;
    PreviewTarget = nullptr;
    Super::NativeDestruct();
}

void UInventoryWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    PreviewElapsed += DeltaTime;
    if (PreviewElapsed >= .066f)
    {
        PreviewElapsed = 0.f;
        UpdateCharacterPreview();
    }
}

void UInventoryWidget::UpdateCharacterPreview()
{
    auto* Character = Cast<ABaseCharacter>(GetOwningPlayerPawn());
    if (!Character) return;
    Text_AttackValue->SetText(FText::AsNumber(FMath::RoundToInt(Character->GetAttackDamage())));
    Text_HealthValue->SetText(FText::AsNumber(FMath::RoundToInt(Character->GetMaxHealth())));
    Text_SpeedValue->SetText(FText::AsNumber(FMath::RoundToInt(Character->GetCharacterMovement()->MaxWalkSpeed)));
    if (!IsValid(PreviewActor)) return;
    auto* Capture = PreviewActor->GetCaptureComponent2D();
    if (!IsValid(PreviewMesh)) return;
    PreviewMesh->UpdateBounds();
    const FVector Center = PreviewMesh->Bounds.Origin + FVector(0.f, 0.f, 50.f);
    const float Distance = FMath::Max(100.f, PreviewMesh->Bounds.BoxExtent.Z * 1.8f);
    const FVector Camera = Center + FVector(Distance, 0, 15);
    PreviewActor->SetActorLocationAndRotation(Camera, (Center - Camera).Rotation());
    Capture->CaptureScene();
}
