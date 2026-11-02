#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Data/ItemDataStructs.h"
#include "Styling/SlateBrush.h"
#include "InventoryWidget.generated.h"

class UButton;
class UImage;
class USpinBox;
class UTextBlock;
class UBorder;
class UUniformGridPanel;
class UCanvasPanel;
class UTexture2D;
class UTextureRenderTarget2D;
class USceneCaptureComponent2D;
class ASceneCapture2D;
class USkeletalMeshComponent;
class UInventoryComponent;
class UDragDropOperation;
class UItemBase;

// WBP 표시용 스냅샷. 실제 아이템과 수량은 InventoryComponent가 관리한다.
USTRUCT(BlueprintType)
struct FInventoryItemView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText TooltipText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0")) float UnitWeight = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventorySlotClicked, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryItemSelected, int32, SlotIndex, FName, ItemId);

UCLASS()
class SIA_API UInventoryItemDragOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UInventoryComponent> Inventory;
    UPROPERTY() int32 SlotIndex = INDEX_NONE;
    UPROPERTY() bool bFromEquipment = false;
    UPROPERTY() EEquipmentSlot EquipmentSlot = EEquipmentSlot::None;
};

UCLASS()
class SIA_API UInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    friend class FInventoryDragCancellationTest;
    UInventorySlotWidget(const FObjectInitializer& ObjectInitializer);
    void SetItem(int32 Index, const FInventoryItemView& Item);
    void SetEquipmentItem(EEquipmentSlot EquipmentSlot, UInventoryComponent* Inventory,
        UTexture2D* FallbackIcon, const FText& EmptyLabel);
    void SetDragInventory(UInventoryComponent* Inventory) { DragInventory = Inventory; }
    void SetSelected(bool bSelected);
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventorySlotClicked OnSlotClicked;
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventorySlotClicked OnSlotUseRequested;
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventorySlotClicked OnSlotEquipRequested;
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventorySlotClicked OnSlotContextRequested;
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent, UDragDropOperation*& Operation) override;
    virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SlotButton;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> SlotBackground;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> ItemIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> QuantityText;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> SlotIndexText;
private:
    UFUNCTION() void HandleClicked();
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Appearance") TSubclassOf<UUserWidget> ItemTooltipClass;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Appearance") FSlateBrush SelectionBrush;
    UPROPERTY(Transient) FSlateBrush NormalBrush;
    int32 SlotIndex = INDEX_NONE;
    bool bEquipmentSlot = false;
    EEquipmentSlot EquipmentSlot = EEquipmentSlot::None;
    UPROPERTY(Transient) TObjectPtr<UInventoryComponent> DragInventory;
};

UCLASS()
class SIA_API UInventoryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    friend class FInventoryDropSafetyTest;
    UInventoryWidget(const FObjectInitializer& ObjectInitializer);
    // Array index is the slot index. Empty records represent unoccupied slots.
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetItems(const TArray<FInventoryItemView>& Items);
    UFUNCTION(BlueprintCallable, Category = "Inventory|UI") void BindInventory(UInventoryComponent* Inventory);
    void RequestDropItemForSlot(int32 InventorySlot);
    void ShowInventoryMessage(const FText& Message);
    void CloseItemContextMenu();
    UFUNCTION(BlueprintPure, Category = "Inventory|UI") UInventoryComponent* GetBoundInventory() const { return BoundInventory; }
    UPROPERTY(BlueprintAssignable, Category = "Inventory|UI") FInventoryItemSelected OnItemSelected;
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout")
    TSoftClassPtr<UInventorySlotWidget> SlotWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "1", ClampMax = "120"))
    int32 SlotCount = 24;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Layout", meta = (ClampMin = "1"))
    int32 Columns = 4;
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    virtual bool NativeOnDrop(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent, UDragDropOperation* Operation) override;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UUniformGridPanel> ItemGrid;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CapacityText;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectionName;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectionDescription;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SelectionQuantity;
    UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> InventoryHint;
private:
    UFUNCTION() void RefreshFromInventory();
    UFUNCTION() void HandleSlotUseRequested(int32 Index);
    UFUNCTION() void HandleSlotEquipRequested(int32 Index);
    UFUNCTION() void HandleSlotContextRequested(int32 Index);
    UFUNCTION() void HandleContextUse();
    UFUNCTION() void HandleContextDrop();
    UFUNCTION() void ShowContextQuickSlots();
    UFUNCTION() void ShowContextOptions();
    UFUNCTION() void AssignContextQuickSlot1();
    UFUNCTION() void AssignContextQuickSlot2();
    UFUNCTION() void AssignContextQuickSlot3();
    UFUNCTION() void AssignContextQuickSlot4();
    void AssignContextQuickSlot(int32 QuickSlotIndex);
    void PositionContextMenu(float MenuHeight);
    UPROPERTY(Transient) TObjectPtr<UInventoryComponent> BoundInventory;
    float DisplayMaxWeight = 50.f;
    void UpdateCharacterPreview();
    UFUNCTION() void FocusInventory();
    UFUNCTION() void FocusEquipment();
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> Image_CharacterPreview;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_AttackValue;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_HealthValue;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SpeedValue;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_WeightValue;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_InventoryTab;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> Button_EquipmentTab;
    UPROPERTY(Transient) TObjectPtr<ASceneCapture2D> PreviewActor;
    UPROPERTY(Transient) TObjectPtr<AActor> PreviewCharacterActor;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> PreviewMesh;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> PreviewTarget;
    UPROPERTY(Transient) TObjectPtr<UTexture2D> FallbackEquipmentIcon;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Head;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Chest;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Hands;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Legs;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Feet;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Neck;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Accessory;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Ring;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UInventorySlotWidget> EquipmentItemSlot_Weapon;
    float PreviewElapsed = 0.f;
    UFUNCTION() void HandleSlotClicked(int32 Index);
    UFUNCTION() void CloseInventory();
    void RefreshSelection();
    void RefreshEquipmentDisplay();
    void BuildSlots();
    UPROPERTY(Transient) TArray<TObjectPtr<UInventorySlotWidget>> Slots;
    UPROPERTY(Transient) TArray<FInventoryItemView> DisplayItems;
    int32 SelectedIndex = INDEX_NONE;
    int32 ContextItemIndex = INDEX_NONE;
    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> InventoryRoot;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|UI") TSubclassOf<UUserWidget> ItemContextMenuClass;
    UPROPERTY(Transient) TObjectPtr<UUserWidget> ItemContextMenu;
    UPROPERTY(Transient) TObjectPtr<UButton> ContextUseButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ContextShortcutButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ContextDropButton;
    UPROPERTY(Transient) TObjectPtr<UButton> ContextBackButton;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ContextQuickSlotButtons;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> WBP_DropBackdrop;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<USpinBox> WBP_DropCount;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> WBP_DropName;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> WBP_DropConfirm;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> WBP_DropCancel;
    int32 PendingDropSlot = INDEX_NONE;
    UPROPERTY(Transient) TWeakObjectPtr<UItemBase> PendingDropItem;
    void CloseDropDialog();
    UFUNCTION() void ConfirmDropItem();
    UFUNCTION() void CancelDropItem();
};
