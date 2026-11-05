#pragma once

#include "CoreMinimal.h"
#include "World/HarvestableResource.h"
#include "RockResource.generated.h"

class UStaticMeshComponent;
class UGeometryCollectionComponent;

UCLASS(Blueprintable)
class SIA_API ARockResource : public AHarvestableResource
{
	GENERATED_BODY()

public:
	ARockResource();

protected:
	virtual bool ConfigureResourcePresentation(FText& OutError) override;
	virtual void OnAcceptedHit(const FHarvestHitContext& Hit, bool bWasLethal) override;
	virtual void OnResourceDepleted(const FHarvestHitContext& Hit) override;

private:
	void ReleaseNextPieces(int32 Count);

	UPROPERTY(Transient)
	TObjectPtr<UGeometryCollectionComponent> DestructionComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PieceComponents;

	int32 SelectedVariationIndex = INDEX_NONE;
	int32 NextPieceIndex = 0;
};
