#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HarvestDebris.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class SIA_API AHarvestDebris : public AActor
{
	GENERATED_BODY()

public:
	AHarvestDebris();
	void InitializeDebris(UStaticMesh* Mesh, const FVector& MeshScale, const FVector& Impulse,
		float LifeSpanSeconds);

private:
	UPROPERTY(VisibleAnywhere, Category = "Harvest|Debris")
	TObjectPtr<UBoxComponent> PhysicsBody;

	UPROPERTY(VisibleAnywhere, Category = "Harvest|Debris")
	TObjectPtr<UStaticMeshComponent> MeshVisual;
};
