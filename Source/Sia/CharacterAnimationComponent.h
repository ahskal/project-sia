#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterAnimationComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;

UCLASS(ClassGroup = (Character), meta = (BlueprintSpawnableComponent))
class SIA_API UCharacterAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterAnimationComponent();

	float PlayComboAttack(int32 AttackIndex);
	void StopComboAttack(float BlendOutTime = 0.12f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> AnimationClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Combat")
	TArray<TObjectPtr<UAnimSequenceBase>> ComboAttacks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Combat")
	FName AttackSlotName = TEXT("DefaultSlot");

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage;

};
