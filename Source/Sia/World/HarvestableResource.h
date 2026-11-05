#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/HarvestDataTypes.h"
#include "HarvestableResource.generated.h"

class UDataTable;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Abstract, Blueprintable)
class SIA_API AHarvestableResource : public AActor
{
	GENERATED_BODY()

public:
	AHarvestableResource();
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	float ReceiveHarvestHit(const FHarvestHitContext& Hit);
	UFUNCTION(BlueprintPure, Category = "Harvest")
	float GetRemainingHealth() const { return RemainingHealth; }
	UFUNCTION(BlueprintPure, Category = "Harvest")
	float GetMaximumHealth() const { return Profile ? Profile->MaxHealth : 0.f; }
	UFUNCTION(BlueprintPure, Category = "Harvest")
	bool IsDepleted() const { return bDepleted; }
	UFUNCTION(BlueprintPure, Category = "Harvest")
	float GetResourceSize() const { return ResourceSize; }

protected:
	virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Harvest")
	TObjectPtr<UHarvestProfile> Profile;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Harvest", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float ResourceSize = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harvest")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harvest")
	TObjectPtr<UStaticMeshComponent> ResourceMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Harvest")
	TObjectPtr<UBoxComponent> ResourceCollision;

	virtual bool ConfigureResourcePresentation(FText& OutError);
	virtual void OnAcceptedHit(const FHarvestHitContext& Hit, bool bWasLethal);
	virtual void OnResourceDepleted(const FHarvestHitContext& Hit);

	void SpawnDebris(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation,
		const FVector& Scale, const FVector& Impulse, float LifeSpanSeconds);
	void SetResourceCollisionBounds(const FVector& Center, const FVector& Extent);
	void DisableResourceCollision();
	UDataTable* GetItemDataTable(const FHarvestHitContext& Hit) const;

private:
	void SpawnResolvedDrops(UDataTable* ItemTable);

	UPROPERTY(Transient)
	float RemainingHealth = 0.f;

	UPROPERTY(Transient)
	bool bDepleted = false;

	UPROPERTY(Transient)
	bool bProfileReady = false;

	FRandomStream DropRandom;
};
