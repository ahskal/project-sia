#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageTrap.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class SIA_API ADamageTrap : public AActor
{
    GENERATED_BODY()

public:
    ADamageTrap();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap")
    TObjectPtr<UBoxComponent> Trigger;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap")
    TObjectPtr<UStaticMeshComponent> Plate;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trap")
    TObjectPtr<UTextRenderComponent> WarningText;

private:
    // Only the character capsule counts, so meshes and attack hitboxes cannot retrigger damage.
    UFUNCTION()
    void OnTrapEntered(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnTrapExited(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    TSet<TWeakObjectPtr<AActor>> Occupants;
};
