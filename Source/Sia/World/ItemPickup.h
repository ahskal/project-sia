#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "ItemPickup.generated.h"

class ABaseCharacter;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class SIA_API AItemPickup : public AActor
{
    GENERATED_BODY()
public:
    AItemPickup();
    virtual void OnConstruction(const FTransform& Transform) override;
    UFUNCTION(BlueprintCallable, Category = "Item") int32 TryPickup(ABaseCharacter* Character);
    void RefreshForInventoryDrop();
    UFUNCTION(BlueprintPure, Category = "Item") float GetPickupRange() const { return PickupRange; }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (RowType = "/Script/Sia.ItemData")) FDataTableRowHandle ItemRow;
    // 월드 수량은 스택 상한보다 커도 된다. 획득할 때 여러 슬롯으로 분배한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1")) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1")) float PickupRange = 180.f;
protected:
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item") TObjectPtr<UStaticMeshComponent> ItemMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item") TObjectPtr<UTextRenderComponent> Label;
private:
    void RefreshVisuals();
};
