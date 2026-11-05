#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Data/HarvestDataTypes.h"
#include "ItemDataStructs.generated.h"

class UTexture2D;
class UStaticMesh;

UENUM(BlueprintType)
enum class EItemType : uint8
{
    Consumable UMETA(DisplayName = "소비품"),
    Material UMETA(DisplayName = "재료"),
    Weapon UMETA(DisplayName = "무기"),
    Armor UMETA(DisplayName = "방어구"),
    Quest UMETA(DisplayName = "퀘스트")
};

UENUM(BlueprintType)
enum class EItemQuality : uint8
{
    Common UMETA(DisplayName = "일반"),
    Uncommon UMETA(DisplayName = "고급"),
    Rare UMETA(DisplayName = "희귀"),
    Epic UMETA(DisplayName = "영웅"),
    Legendary UMETA(DisplayName = "전설")
};

UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    None UMETA(DisplayName = "없음"),
    Head UMETA(DisplayName = "머리"),
    Chest UMETA(DisplayName = "몸통"),
    Hands UMETA(DisplayName = "손"),
    Legs UMETA(DisplayName = "다리"),
    Feet UMETA(DisplayName = "발"),
    Neck UMETA(DisplayName = "목걸이"),
    Accessory UMETA(DisplayName = "장신구"),
    Ring UMETA(DisplayName = "반지"),
    Weapon UMETA(DisplayName = "무기")
};

USTRUCT(BlueprintType)
struct SIA_API FItemStatistics
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) float ArmorRating = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) float DamageValue = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) float RestorationAmount = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) int32 SellValue = 0;
};

USTRUCT(BlueprintType)
struct SIA_API FItemTextData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = true)) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText UsageText;
};

USTRUCT(BlueprintType)
struct SIA_API FItemNumericData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) float Weight = 0.f;
    // 1이면 스택 불가. 별도 bool을 두어 설정이 충돌하는 것을 방지한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 MaxStackSize = 1;
};

USTRUCT(BlueprintType)
struct SIA_API FItemAssetData
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> Mesh;
};

// 행 이름(RowName)이 아이템 ID다. 보유 수량은 테이블에 저장하지 않는다.
USTRUCT(BlueprintType)
struct SIA_API FItemData : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") EItemType ItemType = EItemType::Material;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") EItemQuality ItemQuality = EItemQuality::Common;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") EEquipmentSlot EquipmentSlot = EEquipmentSlot::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FItemStatistics ItemStatistics;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FItemTextData TextData;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FItemNumericData NumericData;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item") FItemAssetData AssetData;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Tool") EHarvestToolType HarvestToolType = EHarvestToolType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Tool")
    FTransform ToolGripTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(.55f));

    bool IsValidDefinition() const
    {
        return !TextData.DisplayName.IsEmpty() && NumericData.MaxStackSize >= 1
            && FMath::IsFinite(NumericData.Weight) && NumericData.Weight >= 0.f
            && FMath::IsFinite(ItemStatistics.RestorationAmount) && ItemStatistics.RestorationAmount >= 0.f
            && FMath::IsFinite(ItemStatistics.ArmorRating) && ItemStatistics.ArmorRating >= 0.f
            && FMath::IsFinite(ItemStatistics.DamageValue) && ItemStatistics.DamageValue >= 0.f
            && (HarvestToolType == EHarvestToolType::None
                || (ItemType == EItemType::Weapon && EquipmentSlot == EEquipmentSlot::Weapon
                    && !AssetData.Mesh.IsNull() && ToolGripTransform.IsValid()
                    && ToolGripTransform.GetScale3D().GetMin() > 0.f));
    }
};

USTRUCT(BlueprintType)
struct SIA_API FInventoryInitialItem
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1")) int32 Quantity = 1;
};
