#pragma once

#include "CoreMinimal.h"
#include "Data/ItemDataStructs.h"
#include "UObject/Object.h"
#include "ItemBase.generated.h"

class UInventoryComponent;

// 인벤토리 한 슬롯의 런타임 스택. 수량 변경은 InventoryComponent에서만 수행한다.
UCLASS(BlueprintType)
class SIA_API UItemBase : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category = "Item") FName GetItemId() const { return ItemId; }
    UFUNCTION(BlueprintPure, Category = "Item") int32 GetQuantity() const { return Quantity; }
    UFUNCTION(BlueprintPure, Category = "Item") FItemData GetItemData() const { return Data; }
    UFUNCTION(BlueprintPure, Category = "Item") double GetStackWeight() const { return double(Data.NumericData.Weight) * Quantity; }
    const FItemData& GetData() const { return Data; }

private:
    friend class UInventoryComponent;
    UPROPERTY(VisibleAnywhere, Category = "Item") FName ItemId = NAME_None;
    UPROPERTY(VisibleAnywhere, Category = "Item") int32 Quantity = 0;
    UPROPERTY(VisibleAnywhere, Category = "Item") FItemData Data;
};
