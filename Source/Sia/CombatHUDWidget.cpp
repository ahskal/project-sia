#include "CombatHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/InventoryComponent.h"
#include "BaseCharacter.h"
#include "InventoryWidget.h"
#include "Items/ItemBase.h"
#include "Styling/CoreStyle.h"

void UCombatHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    SetHealth(100.f, 100.f);
}

void UCombatHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetOwningPlayerPawn()))
        BoundInventory = Character->GetInventory();
    if (BoundInventory) BoundInventory->OnInventoryChanged.AddUniqueDynamic(this, &UCombatHUDWidget::RefreshQuickSlots);
    RefreshQuickSlots();
}

void UCombatHUDWidget::NativeDestruct()
{
    if (BoundInventory) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UCombatHUDWidget::RefreshQuickSlots);
    BoundInventory = nullptr;
    Super::NativeDestruct();
}

void UCombatHUDWidget::BindInventory(UInventoryComponent* Inventory)
{
    if (BoundInventory) BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UCombatHUDWidget::RefreshQuickSlots);
    BoundInventory = Inventory;
    if (BoundInventory) BoundInventory->OnInventoryChanged.AddUniqueDynamic(this, &UCombatHUDWidget::RefreshQuickSlots);
    RefreshQuickSlots();
}

void UCombatHUDWidget::RefreshQuickSlots()
{
    UTextBlock* Labels[] = { WBP_QuickSlotLabel_1, WBP_QuickSlotLabel_2,
        WBP_QuickSlotLabel_3, WBP_QuickSlotLabel_4 };
    UImage* Icons[] = { WBP_QuickSlotIcon_1, WBP_QuickSlotIcon_2,
        WBP_QuickSlotIcon_3, WBP_QuickSlotIcon_4 };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (!Labels[Index]) continue;
        const FName ItemId = BoundInventory ? BoundInventory->GetQuickSlotItemId(Index) : NAME_None;
        int32 Quantity = 0;
        UTexture2D* IconTexture = nullptr;
        if (BoundInventory && !ItemId.IsNone())
        {
            Quantity = BoundInventory->GetTotalQuantity(ItemId);
            for (int32 SlotIndex = 0; SlotIndex < BoundInventory->GetMaxSlots(); ++SlotIndex)
            {
                UItemBase* Item = BoundInventory->GetItemAt(SlotIndex);
                if (Item && Item->GetItemId() == ItemId)
                {
                    IconTexture = Item->GetData().AssetData.Icon.LoadSynchronous();
                    break;
                }
            }
        }
        Labels[Index]->SetText(Quantity > 0 ? FText::AsNumber(Quantity) : FText::GetEmpty());
        Labels[Index]->SetVisibility(Quantity > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (Icons[Index])
        {
            Icons[Index]->SetBrushFromTexture(IconTexture);
            Icons[Index]->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        }
    }
}

bool UCombatHUDWidget::NativeOnDrop(const FGeometry&, const FDragDropEvent& Event, UDragDropOperation* Operation)
{
    auto* Drag = Cast<UInventoryItemDragOperation>(Operation);
    if (!Drag || !BoundInventory || Drag->Inventory != BoundInventory) return false;
    return TryAssignQuickSlotAtPosition(Event.GetScreenSpacePosition(), Drag->Inventory, Drag->SlotIndex);
}

bool UCombatHUDWidget::TryAssignQuickSlotAtPosition(const FVector2D& ScreenPosition,
    UInventoryComponent* Inventory, int32 InventorySlot)
{
    if (!Inventory || Inventory != BoundInventory) return false;
    UBorder* QuickSlotBorders[] = { WBP_QuickSlot_1, WBP_QuickSlot_2,
        WBP_QuickSlot_3, WBP_QuickSlot_4 };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(QuickSlotBorders); ++Index)
    {
        if (QuickSlotBorders[Index] && QuickSlotBorders[Index]->GetCachedGeometry().IsUnderLocation(ScreenPosition))
        {
            // AssignQuickSlot broadcasts OnInventoryChanged, which refreshes this HUD.
            return BoundInventory->AssignQuickSlot(Index, InventorySlot);
        }
    }
    return false;
}

void UCombatHUDWidget::SetHealth(float Current, float Maximum)
{
    if (!HealthProgress || !HealthValueText) return;
    const float Ratio = Maximum > 0.f ? FMath::Clamp(Current / Maximum, 0.f, 1.f) : 0.f;
    HealthProgress->SetPercent(Ratio);
    HealthProgress->SetFillColorAndOpacity(Ratio <= 0.25f
        ? FLinearColor(1.f, 0.16f, 0.18f) : FLinearColor(0.12f, 0.8f, 0.48f));
    HealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"),
        FMath::CeilToInt(Current), FMath::CeilToInt(Maximum))));
}

void UCombatHUDWidget::ShowDamage(float Damage, const FVector& WorldPosition)
{
    if (!DamageCanvas || Damage <= 0.f) return;
    if (DamageNumbers.Num() >= 64)
    {
        DamageNumbers[0].Label->RemoveFromParent();
        DamageNumbers.RemoveAt(0);
    }
    FCombatDamageNumber Entry;
    Entry.Label = WidgetTree->ConstructWidget<UTextBlock>();
    Entry.Label->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
    auto Font = FCoreStyle::GetDefaultFontStyle("Bold", 28);
    Font.OutlineSettings.OutlineSize = 2;
    Font.OutlineSettings.OutlineColor = FLinearColor(0.02f, 0.015f, 0.01f, 0.9f);
    Entry.Label->SetFont(Font);
    Entry.Label->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.8f, 0.22f)));
    Entry.Label->SetShadowOffset(FVector2D(1.f, 2.f));
    auto* NumberSlot = DamageCanvas->AddChildToCanvas(Entry.Label);
    NumberSlot->SetAutoSize(true);
    NumberSlot->SetAlignment(FVector2D(0.5f, 1.f));
    NumberSlot->SetZOrder(10);
    Entry.Label->SetVisibility(ESlateVisibility::Hidden);
    Entry.WorldPosition = WorldPosition;
    Entry.HorizontalOffset = FMath::FRandRange(-18.f, 18.f);
    DamageNumbers.Add(Entry);
}

void UCombatHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
    Super::NativeTick(Geometry, DeltaTime);
    for (int32 Index = DamageNumbers.Num() - 1; Index >= 0; --Index)
    {
        auto& Entry = DamageNumbers[Index];
        Entry.Age += DeltaTime;
        if (Entry.Age >= 0.9f)
        {
            Entry.Label->RemoveFromParent();
            DamageNumbers.RemoveAtSwap(Index);
            continue;
        }
        FVector2D Screen;
        const bool bVisible = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            GetOwningPlayer(), Entry.WorldPosition, Screen, true);
        Entry.Label->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
        if (bVisible)
        {
            CastChecked<UCanvasPanelSlot>(Entry.Label->Slot)->SetPosition(
                Screen + FVector2D(Entry.HorizontalOffset, -Entry.Age * 65.f));
            Entry.Label->SetRenderOpacity(1.f - FMath::Clamp((Entry.Age - 0.35f) / 0.55f, 0.f, 1.f));
        }
    }
}
