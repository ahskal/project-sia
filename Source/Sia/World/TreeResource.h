#pragma once

#include "CoreMinimal.h"
#include "World/HarvestableResource.h"
#include "TreeResource.generated.h"

UCLASS(Blueprintable)
class SIA_API ATreeResource : public AHarvestableResource
{
	GENERATED_BODY()

public:
	ATreeResource();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual bool ConfigureResourcePresentation(FText& OutError) override;
	virtual void OnAcceptedHit(const FHarvestHitContext& Hit, bool bWasLethal) override;
	virtual void OnResourceDepleted(const FHarvestHitContext& Hit) override;

private:
	FRotator FallStartRotation = FRotator::ZeroRotator;
	FRotator FallTargetRotation = FRotator::ZeroRotator;
	float FallElapsed = 0.f;
	bool bFalling = false;
};
