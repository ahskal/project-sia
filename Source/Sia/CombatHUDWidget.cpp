#include "CombatHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UCombatHUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    SetHealth(100.f, 100.f);
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
