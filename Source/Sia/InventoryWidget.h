#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UBorder;
class UUniformGridPanel;
class UTexture2D;

// Presentation data only. The future inventory system owns item storage and rules.
USTRUCT(BlueprintType)
struct FInventoryItemView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventorySlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryItemSelected, int32, SlotIndex, FName, ItemId);

UCLASS()
class SIA_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetItem(int32 Index, const FInventoryItemView& Item);
    void SetSelected(bool bSelected);
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventorySlotClicked OnSlotClicked;
protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SlotButton;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> SlotBackground;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> ItemIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> QuantityText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SlotIndexText;
private:
    UFUNCTION() void HandleClicked();
    int32 SlotIndex = INDEX_NONE;
};

UCLASS()
class SIA_API UInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UInventoryWidget(const FObjectInitializer& ObjectInitializer);
    // Array index is the slot index. Empty records represent unoccupied slots.
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetItems(const TArray<FInventoryItemView>& Items);
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventoryItemSelected OnItemSelected;
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout")
    TSoftClassPtr<UInventorySlotWidget> SlotWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "1", ClampMax = "120"))
    int32 SlotCount = 30;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "1"))
    int32 Columns = 6;
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UUniformGridPanel> ItemGrid;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CapacityText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SelectionName;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SelectionDescription;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SelectionQuantity;
private:
    UFUNCTION() void HandleSlotClicked(int32 Index);
    UFUNCTION() void CloseInventory();
    void RefreshSelection();
    void BuildSlots();
    UPROPERTY(Transient) TArray<TObjectPtr<UInventorySlotWidget>> Slots;
    UPROPERTY(Transient) TArray<FInventoryItemView> DisplayItems;
    int32 SelectedIndex = INDEX_NONE;
};
