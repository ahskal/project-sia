#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "HarvestDataTypes.generated.h"

class UDataTable;
class UGeometryCollection;
class UStaticMesh;
class AActor;

UENUM(BlueprintType)
enum class EHarvestToolType : uint8
{
	None,
	Axe,
	Hammer,
	Pickaxe,
	Hoe,
	Shovel
};

UENUM(BlueprintType)
enum class EHarvestResourceType : uint8
{
	Tree,
	Rock
};

USTRUCT(BlueprintType)
struct SIA_API FHarvestDropRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Drops")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Drops", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Probability = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Drops", meta = (ClampMin = "0"))
	int32 MinAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Drops", meta = (ClampMin = "0"))
	int32 MaxAmount = 1;
};

USTRUCT(BlueprintType)
struct SIA_API FHarvestResolvedDrop
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Harvest|Drops")
	FName ItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Harvest|Drops")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct SIA_API FHarvestRockPiece
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock")
	FTransform RelativeTransform = FTransform::Identity;
};

UCLASS(BlueprintType)
class SIA_API UHarvestProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = "0.01"))
	float MaxHealth = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Presentation")
	TArray<TObjectPtr<UStaticMesh>> ResourceMeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tools")
	EHarvestToolType PreferredTool = EHarvestToolType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tools", meta = (ClampMin = "1.0"))
	float PreferredToolDamageMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Drops", meta = (ClampMin = "0.01"))
	float SizeYieldExponent = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Drops")
	TArray<FHarvestDropRule> Drops;

	bool IsValidForItemTable(const UDataTable* ItemTable, FText& OutError) const;
	virtual bool HasValidResourcePresentation(FText& OutError) const;
};

UCLASS(BlueprintType)
class SIA_API UTreeHarvestProfile : public UHarvestProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree")
	TObjectPtr<UStaticMesh> HitChipMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "1.0", Units = "cm"))
	float TrunkCollisionRadius = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "1.0", Units = "cm"))
	float TrunkCollisionHeight = 320.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "1", ClampMax = "8"))
	int32 ChipsPerHit = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float ChipScale = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "0.0", ClampMax = "50.0", Units = "cm"))
	float ChipRadius = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "0.0"))
	float ChipImpulse = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "0.1"))
	float FallDuration = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "30.0", ClampMax = "120.0"))
	float FallAngleDegrees = 88.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Tree", meta = (ClampMin = "0.1"))
	float CleanupDelaySeconds = 4.f;

	virtual bool HasValidResourcePresentation(FText& OutError) const override;
};

UCLASS(BlueprintType)
class SIA_API URockHarvestProfile : public UHarvestProfile
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock|Collision", meta = (ClampMin = "1.0"))
	float CollisionVerticalScale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock|Destruction")
	TArray<TObjectPtr<UGeometryCollection>> DestructionVariants;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock")
	TArray<FHarvestRockPiece> Pieces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PiecesReleasedPerHit = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock", meta = (ClampMin = "0.0"))
	float PieceImpulse = 550.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Harvest|Rock", meta = (ClampMin = "0.1"))
	float CleanupDelaySeconds = 3.f;

	virtual bool HasValidResourcePresentation(FText& OutError) const override;
};

struct SIA_API FHarvestHitContext
{
	float BaseDamage = 0.f;
	EHarvestToolType ToolType = EHarvestToolType::None;
	FVector ImpactPoint = FVector::ZeroVector;
	FVector ImpactNormal = FVector::UpVector;
	FVector AttackDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> Attacker;
};

namespace SiaHarvesting
{
	SIA_API float ResolveDamage(float BaseDamage, EHarvestToolType ToolType,
		EHarvestToolType PreferredTool, float PreferredToolMultiplier);

	SIA_API TArray<FHarvestResolvedDrop> ResolveDrops(const TArray<FHarvestDropRule>& Rules,
		float ResourceSize, float YieldExponent, FRandomStream& Random);
}
