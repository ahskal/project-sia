
#include "BaseCharacter.h"
#include "CharacterAnimationComponent.h"

#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/SkeletalMesh.h"

ABaseCharacter::ABaseCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    // Character Movement
    bUseControllerRotationYaw = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

    // Skeletal Mesh
    GetMesh()->SetRelativeLocation(FVector(-90.f, 0.f, 0.f));
    GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

    // Camera Boom
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
        TEXT("CameraBoom")
    );

    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 350.f;
    CameraBoom->bUsePawnControlRotation = true;

    // Camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(
        TEXT("FollowCamera")
    );

    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName
    );

    FollowCamera->bUsePawnControlRotation = false;

    // Animation Component
    AnimationComponent =
        CreateDefaultSubobject<UCharacterAnimationComponent>(
            TEXT("CharacterAnimationComponent")
        );

    if (AnimationComponent->AnimationClass)
    {
        GetMesh()->SetAnimationMode(
            EAnimationMode::AnimationBlueprint
        );

        GetMesh()->SetAnimInstanceClass(
            AnimationComponent->AnimationClass
        );
    }

    // Attack Hitbox
    AttackHitbox = CreateDefaultSubobject<UBoxComponent>(
        TEXT("AttackHitbox")
    );

    AttackHitbox->SetupAttachment(
        GetMesh(),
        TEXT("AttackSocket")
    );

    AttackHitbox->SetRelativeLocation(FVector(8.f, 0.f, 0.f));
    AttackHitbox->SetBoxExtent(FVector(12.f, 8.f, 8.f));

    AttackHitbox->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );

    AttackHitbox->SetGenerateOverlapEvents(true);
    AttackHitbox->SetCollisionObjectType(ECC_Pawn);
    AttackHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    AttackHitbox->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap
    );

}

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 기본 Skeletal Mesh
    if (!GetMesh()->GetSkeletalMeshAsset())
    {
        USkeletalMesh* DefaultMesh = LoadObject<USkeletalMesh>(
            nullptr,
            TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple")
        );

        if (DefaultMesh)
        {
            GetMesh()->SetSkeletalMesh(DefaultMesh);
        }
    }

    // Animation Blueprint
    if (AnimationComponent && !GetMesh()->GetAnimClass())
    {
        if (!AnimationComponent->AnimationClass)
        {
            AnimationComponent->AnimationClass =
                LoadClass<UAnimInstance>(
                    nullptr,
                    TEXT("/Game/ABP_Sia.ABP_Sia_C")
                );
        }

        if (AnimationComponent->AnimationClass)
        {
            GetMesh()->SetAnimationMode(
                EAnimationMode::AnimationBlueprint
            );

            GetMesh()->SetAnimInstanceClass(
                AnimationComponent->AnimationClass
            );
        }
    }

    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->JumpZVelocity = 500.f;

}

// =======================================
// Movement
// =======================================

void ABaseCharacter::MoveCharacter(const FVector2D& Input)
{
    MoveInput = Input;

    if (!Controller)
        return;

    const FRotator YawRotation(
        0.f,
        Controller->GetControlRotation().Yaw,
        0.f
    );

    const FVector Forward =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

    const FVector Right =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Input.Y);
    AddMovementInput(Right, Input.X);
}

// =======================================
// Dash
// =======================================

void ABaseCharacter::Dash()
{
    if (!bCanDash || !Controller)
        return;

    if (!GetCharacterMovement()->IsMovingOnGround())
        return;

    const FRotator YawRotation(
        0.f,
        Controller->GetControlRotation().Yaw,
        0.f
    );

    const FVector Forward =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

    const FVector Right =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    const FVector Direction =
        (Forward * MoveInput.Y + Right * MoveInput.X)
        .GetSafeNormal();

    if (Direction.IsNearlyZero())
        return;

    LaunchCharacter(
        Direction * DashSpeed,
        true,
        false
    );

    bCanDash = false;

    GetWorldTimerManager().SetTimer(
        DashCooldownHandle,
        this,
        &ABaseCharacter::ResetDash,
        FMath::Max(DashCooldown, 0.01f),
        false
    );
}

void ABaseCharacter::ResetDash()
{
    bCanDash = true;
}

// =======================================
// Attack
// =======================================

void ABaseCharacter::Attack()
{
    if (!AnimationComponent || !bCanAttack || bIsAttack)
        return;

    bIsAttack = true;
    bCanAttack = false;

    GetWorldTimerManager().SetTimer(
        AttackCooldownHandle,
        this,
        &ABaseCharacter::ResetAttack,
        FMath::Max(AttackCooldown, 0.01f),
        false
    );
}

void ABaseCharacter::ResetAttack()
{
    bCanAttack = true;
    bIsAttack = false;

    DisableAttackHitbox();
}

// =======================================
// Attack Collision
// =======================================

void ABaseCharacter::EnableAttackHitbox()
{
    if (!IsValid(AttackHitbox))
        return;

    AttackHitbox->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
}

void ABaseCharacter::DisableAttackHitbox()
{
    if (!IsValid(AttackHitbox))
        return;

    AttackHitbox->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
}
