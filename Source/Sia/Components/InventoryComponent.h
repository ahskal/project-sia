#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ItemDataStructs.h"
#include "InventoryComponent.generated.h"

class UItemBase;

UENUM(BlueprintType)
enum class EItemAddResult : uint8 { None, Partial, All };

USTRUCT(BlueprintType)
struct SIA_API FItemAddResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 AmountRequested = 0;
    UPROPERTY(BlueprintReadOnly) int32 AmountAdded = 0;
    UPROPERTY(BlueprintReadOnly) EItemAddResult Result = EItemAddResult::None;
    UPROPERTY(BlueprintReadOnly) FText Message;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class SIA_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UInventoryComponent();

    // 기존 아이템이 없는 경우에만 테이블과 용량을 재설정할 수 있다.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool InitializeInventory(UDataTable* Table, int32 InMaxSlots = 24, float InMaxWeight = 50.f);
    UFUNCTION(BlueprintCallable, Category = "Inventory") FItemAddResult TryAddItem(FName ItemId, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category = "Inventory") int32 RemoveFromSlot(int32 SlotIndex, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category = "Inventory") int32 RemoveItem(FName ItemId, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category = "Inventory") bool DropItemFromSlot(int32 SlotIndex, int32 Quantity = 1);
    UFUNCTION(BlueprintCallable, Category = "Inventory") bool DropEquippedItem(EEquipmentSlot EquipmentSlot);
    UFUNCTION(BlueprintCallable, Category = "Inventory") bool UseItem(int32 SlotIndex, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Slots") bool AssignQuickSlot(int32 QuickSlotIndex, int32 InventorySlot);
    UFUNCTION(BlueprintCallable, Category = "Inventory|Quick Slots") bool UseQuickSlot(int32 QuickSlotIndex, FText& OutMessage);
    UFUNCTION(BlueprintPure, Category = "Inventory|Quick Slots") FName GetQuickSlotItemId(int32 QuickSlotIndex) const;
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment") bool EquipItem(int32 InventorySlot, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment") bool EquipItemToSlot(int32 InventorySlot, EEquipmentSlot EquipmentSlot, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment") bool UnequipItem(EEquipmentSlot EquipmentSlot, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment") bool UnequipItemToSlot(EEquipmentSlot EquipmentSlot, int32 InventorySlot, FText& OutMessage);
    UFUNCTION(BlueprintPure, Category = "Inventory|Equipment") UItemBase* GetEquippedItem(EEquipmentSlot EquipmentSlot) const;
    UFUNCTION(BlueprintCallable, Category = "Inventory") bool MoveOrMerge(int32 FromSlot, int32 ToSlot);
    UFUNCTION(BlueprintCallable, Category = "Inventory") bool SplitStack(int32 SlotIndex, int32 Quantity);
    UFUNCTION(BlueprintPure, Category = "Inventory") UItemBase* GetItemAt(int32 SlotIndex) const;
    UFUNCTION(BlueprintPure, Category = "Inventory") int32 GetTotalQuantity(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category = "Inventory") int32 GetOccupiedSlotCount() const;
    UFUNCTION(BlueprintPure, Category = "Inventory") int32 GetMaxSlots() const { return MaxSlots; }
    UFUNCTION(BlueprintPure, Category = "Inventory") float GetMaxWeight() const { return MaxWeight; }
    UFUNCTION(BlueprintPure, Category = "Inventory") double GetCurrentWeight() const { return CurrentWeight; }
    UFUNCTION(BlueprintPure, Category = "Inventory") UDataTable* GetItemDataTable() const;
    const TArray<TObjectPtr<UItemBase>>& GetSlots() const { return Slots; }

    UPROPERTY(BlueprintAssignable, Category = "Inventory") FOnInventoryChanged OnInventoryChanged;

protected:
    virtual void BeginPlay() override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory") TSoftObjectPtr<UDataTable> ItemDataTable;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "120")) int32 MaxSlots = 24;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0")) float MaxWeight = 50.f;
    UPROPERTY(EditDefaultsOnly, Category = "Inventory") TArray<FInventoryInitialItem> InitialItems;

private:
    void EnsureSlots();
    void CommitChange();
    UItemBase* CreateStack(FName ItemId, const FItemData& Data, int32 Quantity);
    UPROPERTY(Transient) TArray<TObjectPtr<UItemBase>> Slots;
    UPROPERTY(Transient) TArray<TObjectPtr<UItemBase>> EquippedItems;
    UPROPERTY(Transient) TArray<FName> QuickSlotItemIds;
    UPROPERTY(VisibleInstanceOnly, Category = "Inventory") double CurrentWeight = 0.0;
};
