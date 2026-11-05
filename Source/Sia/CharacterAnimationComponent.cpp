#include "CharacterAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

UCharacterAnimationComponent::UCharacterAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> FirstAttack(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> SecondAttack(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_02.MM_Attack_02"));
	if (FirstAttack.Succeeded())
		ComboAttacks.Add(FirstAttack.Object);
	if (SecondAttack.Succeeded())
		ComboAttacks.Add(SecondAttack.Object);
}

float UCharacterAnimationComponent::PlayComboAttack(const int32 AttackIndex)
{
	if (!ComboAttacks.IsValidIndex(AttackIndex) || !IsValid(ComboAttacks[AttackIndex]))
		return 0.f;

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || AttackSlotName.IsNone())
		return 0.f;

	StopComboAttack(0.05f);
	UAnimSequenceBase* Sequence = ComboAttacks[AttackIndex];
	ActiveAttackMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		Sequence, AttackSlotName, 0.08f, 0.12f, 1.f, 1, -1.f, 0.f);
	return ActiveAttackMontage ? Sequence->GetPlayLength() : 0.f;
}

void UCharacterAnimationComponent::StopComboAttack(const float BlendOutTime)
{
	if (!ActiveAttackMontage)
		return;
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
				AnimInstance->Montage_Stop(FMath::Max(0.f, BlendOutTime), ActiveAttackMontage);
		}
	}
	ActiveAttackMontage = nullptr;
}

