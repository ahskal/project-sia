#include "Data/HarvestDataTypes.h"

#include "Data/ItemDataStructs.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"

namespace
{
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}
}

bool UHarvestProfile::IsValidForItemTable(const UDataTable* ItemTable, FText& OutError) const
{
	if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.f
		|| !FMath::IsFinite(PreferredToolDamageMultiplier) || PreferredToolDamageMultiplier < 1.f
		|| !FMath::IsFinite(SizeYieldExponent) || SizeYieldExponent <= 0.f)
	{
		OutError = FText::FromString(TEXT("수확 프로필의 체력, 도구 배율 또는 크기 배율이 올바르지 않습니다."));
		return false;
	}
    if (!ItemTable || ItemTable->GetRowStruct() != FItemData::StaticStruct())
    {
        OutError = FText::FromString(TEXT("수확 아이템 데이터 테이블이 없거나 행 구조가 맞지 않습니다."));
        return false;
    }
	if (!HasValidResourcePresentation(OutError))
		return false;
	if (Drops.IsEmpty())
	{
		OutError = FText::FromString(TEXT("수확 프로필에 드롭 항목이 없습니다."));
		return false;
	}
	for (const FHarvestDropRule& Rule : Drops)
	{
		if (Rule.ItemId.IsNone() || !FMath::IsFinite(Rule.Probability)
			|| Rule.Probability < 0.f || Rule.Probability > 1.f
			|| Rule.MinAmount < 0 || Rule.MaxAmount < Rule.MinAmount)
		{
			OutError = FText::FromString(TEXT("수확 드롭 규칙의 아이템, 확률 또는 수량 범위가 올바르지 않습니다."));
			return false;
		}
		const FItemData* Item = ItemTable->FindRow<FItemData>(Rule.ItemId, TEXT("HarvestProfile"), false);
		if (!Item || !Item->IsValidDefinition() || Item->AssetData.Mesh.IsNull())
		{
			OutError = FText::Format(FText::FromString(TEXT("수확 드롭 아이템 '{0}'의 데이터 또는 메시가 없습니다.")),
				FText::FromName(Rule.ItemId));
			return false;
		}
	}

	OutError = FText::GetEmpty();
	return true;
}

bool UHarvestProfile::HasValidResourcePresentation(FText& OutError) const
{
	if (ResourceMeshVariants.IsEmpty())
	{
		OutError = FText::FromString(TEXT("수확 프로필에 사용할 자원 메시가 없습니다."));
		return false;
	}
	for (const TObjectPtr<UStaticMesh>& Mesh : ResourceMeshVariants)
	{
		if (!Mesh)
		{
			OutError = FText::FromString(TEXT("수확 프로필의 자원 메시 목록에 비어 있는 항목이 있습니다."));
			return false;
		}
	}
	OutError = FText::GetEmpty();
	return true;
}

bool UTreeHarvestProfile::HasValidResourcePresentation(FText& OutError) const
{
	if (!Super::HasValidResourcePresentation(OutError))
		return false;
	if (!HitChipMesh || ChipsPerHit <= 0 || ChipsPerHit > 8
		|| !FMath::IsFinite(TrunkCollisionRadius) || TrunkCollisionRadius <= 0.f
		|| !FMath::IsFinite(TrunkCollisionHeight) || TrunkCollisionHeight <= 0.f
		|| !FMath::IsFinite(ChipScale) || ChipScale <= 0.f
        || !FMath::IsFinite(ChipRadius) || ChipRadius < 0.f
		|| !FMath::IsFinite(ChipImpulse) || ChipImpulse < 0.f
		|| !FMath::IsFinite(FallDuration) || FallDuration <= 0.f
		|| !FMath::IsFinite(FallAngleDegrees) || FallAngleDegrees < 30.f || FallAngleDegrees > 120.f
		|| !FMath::IsFinite(CleanupDelaySeconds) || CleanupDelaySeconds <= 0.f)
	{
		OutError = FText::FromString(TEXT("나무 수확 프로필의 메시 또는 파괴 설정이 올바르지 않습니다."));
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}

bool URockHarvestProfile::HasValidResourcePresentation(FText& OutError) const
{
	if (!Super::HasValidResourcePresentation(OutError))
		return false;
	if (Pieces.IsEmpty() || PiecesReleasedPerHit <= 0 || PiecesReleasedPerHit > 4
		|| !FMath::IsFinite(PieceImpulse) || PieceImpulse < 0.f
		|| !FMath::IsFinite(CleanupDelaySeconds) || CleanupDelaySeconds <= 0.f)
	{
		OutError = FText::FromString(TEXT("바위 수확 프로필에 유효한 파괴 조각이 없습니다."));
		return false;
	}
	for (const FHarvestRockPiece& Piece : Pieces)
	{
		const FVector Location = Piece.RelativeTransform.GetLocation();
		const FVector Scale = Piece.RelativeTransform.GetScale3D();
		if (!Piece.Mesh || !IsFiniteVector(Location) || !IsFiniteVector(Scale) || Scale.GetMin() <= 0.f)
		{
			OutError = FText::FromString(TEXT("바위 조각 메시 또는 상대 변환이 올바르지 않습니다."));
			return false;
		}
	}
	OutError = FText::GetEmpty();
	return true;
}

float SiaHarvesting::ResolveDamage(const float BaseDamage, const EHarvestToolType ToolType,
	const EHarvestToolType PreferredTool, const float PreferredToolMultiplier)
{
	if (!FMath::IsFinite(BaseDamage) || BaseDamage <= 0.f)
		return 0.f;
	const float Multiplier = ToolType != EHarvestToolType::None && ToolType == PreferredTool
		&& FMath::IsFinite(PreferredToolMultiplier) && PreferredToolMultiplier >= 1.f
		? PreferredToolMultiplier : 1.f;
	return BaseDamage * Multiplier;
}

TArray<FHarvestResolvedDrop> SiaHarvesting::ResolveDrops(const TArray<FHarvestDropRule>& Rules,
	const float ResourceSize, const float YieldExponent, FRandomStream& Random)
{
	TArray<FHarvestResolvedDrop> Resolved;
	if (!FMath::IsFinite(ResourceSize) || ResourceSize <= 0.f
		|| !FMath::IsFinite(YieldExponent) || YieldExponent <= 0.f)
		return Resolved;

	const double SizeMultiplier = FMath::Pow(static_cast<double>(ResourceSize), static_cast<double>(YieldExponent));
	if (!FMath::IsFinite(SizeMultiplier) || SizeMultiplier <= 0.0)
		return Resolved;

	for (const FHarvestDropRule& Rule : Rules)
	{
		if (Rule.ItemId.IsNone() || !FMath::IsFinite(Rule.Probability)
			|| Rule.Probability <= 0.f || Rule.Probability > 1.f
			|| Rule.MinAmount < 0 || Rule.MaxAmount < Rule.MinAmount)
			continue;
		if (Rule.Probability < 1.f && Random.FRand() >= Rule.Probability)
			continue;

		const int32 SampledAmount = Random.RandRange(Rule.MinAmount, Rule.MaxAmount);
		const double ScaledAmount = static_cast<double>(SampledAmount) * SizeMultiplier;
		const int32 Quantity = ScaledAmount >= static_cast<double>(MAX_int32)
			? MAX_int32
			: FMath::Max(0, FMath::FloorToInt(ScaledAmount + 0.5));
		if (Quantity <= 0)
			continue;

		FHarvestResolvedDrop* Existing = Resolved.FindByPredicate([&Rule](const FHarvestResolvedDrop& Drop)
		{
			return Drop.ItemId == Rule.ItemId;
		});
		if (Existing)
			Existing->Quantity = static_cast<int32>(FMath::Min<int64>(MAX_int32, int64(Existing->Quantity) + Quantity));
		else
			Resolved.Add({ Rule.ItemId, Quantity });
	}
	return Resolved;
}
