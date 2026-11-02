#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UBorder;
class UImage;
class UInventoryComponent;
class UDragDropOperation;

USTRUCT()
struct FCombatDamageNumber
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UTextBlock> Label;
    FVector WorldPosition = FVector::ZeroVector;
    float Age = 0.f;
    float HorizontalOffset = 0.f;
};

UCLASS()
class SIA_API UCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetHealth(float Current, float Maximum);
    void ShowDamage(float Damage, const FVector& WorldPosition);
    void BindInventory(UInventoryComponent* Inventory);
    bool TryAssignQuickSlotAtPosition(const FVector2D& ScreenPosition, UInventoryComponent* Inventory, int32 InventorySlot);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
private:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> DamageCanvas;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> HealthProgress;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HealthValueText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> WBP_QuickSlot_1;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> WBP_QuickSlot_2;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> WBP_QuickSlot_3;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> WBP_QuickSlot_4;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> WBP_QuickSlotLabel_1;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> WBP_QuickSlotLabel_2;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> WBP_QuickSlotLabel_3;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> WBP_QuickSlotLabel_4;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> WBP_QuickSlotIcon_1;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> WBP_QuickSlotIcon_2;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> WBP_QuickSlotIcon_3;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> WBP_QuickSlotIcon_4;
    UPROPERTY() TArray<FCombatDamageNumber> DamageNumbers;
    UPROPERTY(Transient) TObjectPtr<UInventoryComponent> BoundInventory;
    UFUNCTION() void RefreshQuickSlots();
};
