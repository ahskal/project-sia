
#include "BaseCharacter.h"
#include "BasePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "CharacterAnimationComponent.h"
#include "Components/InventoryComponent.h"

#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

ABaseCharacter::ABaseCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

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
    AttackHitbox->SetHiddenInGame(true);

    AttackHitbox->SetGenerateOverlapEvents(true);
    AttackHitbox->bDrawOnlyIfSelected = false;
    AttackHitbox->ShapeColor = FColor::Red;
    AttackHitbox->OnComponentBeginOverlap.AddDynamic(
        this, &ABaseCharacter::OnAttackHitboxOverlap);
    AttackHitbox->SetCollisionObjectType(ECC_Pawn);
    AttackHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    AttackHitbox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
    AttackHitbox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    AttackHitbox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    AttackHitbox->SetCollisionResponseToChannel(
        ECC_Pawn,
        ECR_Overlap
    );

}

float ABaseCharacter::RestoreHealth(float Amount)
{
    if (!FMath::IsFinite(Amount) || Amount <= 0.f || Health <= 0.f) return 0.f;
    const float Previous = Health;
    Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
    if (auto* PC = Cast<ABasePlayerController>(GetController())) PC->RefreshHealthUI();
    return Health - Previous;
}

void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();
    MaxHealth = FMath::Max(1.f, MaxHealth);
    Health = MaxHealth;

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
    if (!bCanDash || !Controller || DashDistance <= 0.f || DashDuration <= 0.f)
        return;

    if (!GetCharacterMovement()->IsMovingOnGround()
        || GetCharacterMovement()->GetRootMotionSource(TEXT("Dash")))
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

    const float SafeDuration = FMath::Max(DashDuration, 0.01f);
    const float DashSpeed = DashDistance / SafeDuration;

    TSharedPtr<FRootMotionSource_ConstantForce> DashSource =
        MakeShared<FRootMotionSource_ConstantForce>();
    DashSource->InstanceName = TEXT("Dash");
    DashSource->Priority = 1000;
    DashSource->AccumulateMode = ERootMotionAccumulateMode::Override;
    DashSource->Duration = SafeDuration;
    DashSource->Force = Direction * DashSpeed;
    DashSource->bInLocalSpace = false;
    DashSource->Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
    DashSource->Settings.UnSetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
    // Preserve momentum; CharacterMovement handles braking after the dash.
    DashSource->FinishVelocityParams.Mode =
        ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;

    GetCharacterMovement()->ApplyRootMotionSource(DashSource);

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
    if (Health <= 0.f || !AnimationComponent || !bCanAttack || bIsAttack)
        return;

    HitActorsThisAttack.Reset();
    bIsAttack = true;
    bCanAttack = false;

    // Hitbox activation is controlled by animation notifies.

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

    AttackHitbox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    AttackHitbox->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly
    );
    AttackHitbox->SetHiddenInGame(false);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Attack Hitbox: ON"));
    }

    // Explicit query also detects geometry that does not generate overlap events.
    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
    ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    TArray<FOverlapResult> Results;
    GetWorld()->OverlapMultiByObjectType(
        Results, AttackHitbox->GetComponentLocation(),
        AttackHitbox->GetComponentQuat(), ObjectTypes,
        FCollisionShape::MakeBox(AttackHitbox->GetScaledBoxExtent()), QueryParams);

    TSet<AActor*> ReportedActors;
    for (const FOverlapResult& Result : Results)
    {
        AActor* HitActor = Result.GetActor();
        if (IsValid(HitActor) && !ReportedActors.Contains(HitActor))
        {
            ReportedActors.Add(HitActor);
            OnAttackHitboxOverlap(AttackHitbox, HitActor,
                Result.GetComponent(), INDEX_NONE, false, FHitResult());
        }
    }
}

void ABaseCharacter::DisableAttackHitbox()
{
    if (!IsValid(AttackHitbox))
        return;

    AttackHitbox->SetCollisionEnabled(
        ECollisionEnabled::NoCollision
    );
    AttackHitbox->SetHiddenInGame(true);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Attack Hitbox: OFF"));
    }
}

void ABaseCharacter::OnAttackHitboxOverlap(
    UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bIsAttack || !IsValid(AttackHitbox)
        || AttackHitbox->GetCollisionEnabled() == ECollisionEnabled::NoCollision
        || !IsValid(OtherActor) || OtherActor == this)
        return;

    if (HitActorsThisAttack.Contains(OtherActor))
        return;

    // A constrained target may overlap through its static base first.
    // Select the nearest simulated component so that the dummy receives the impulse.
    UPrimitiveComponent* PhysicsComponent = OtherComp;
    FName PhysicsBone = bFromSweep ? SweepResult.BoneName : NAME_None;
    if (!IsValid(PhysicsComponent) || !PhysicsComponent->IsSimulatingPhysics(PhysicsBone))
    {
        PhysicsComponent = nullptr;
        PhysicsBone = NAME_None;
        TInlineComponentArray<UPrimitiveComponent*> Components(OtherActor);
        double NearestDistance = TNumericLimits<double>::Max();
        for (UPrimitiveComponent* Component : Components)
        {
            if (!Component->IsSimulatingPhysics())
                continue;
            const double Distance = FVector::DistSquared(
                Component->Bounds.Origin, AttackHitbox->GetComponentLocation());
            if (Distance < NearestDistance)
            {
                NearestDistance = Distance;
                PhysicsComponent = Component;
            }
        }
    }

    ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
    const bool bDamageable = OtherActor->CanBeDamaged()
        && (Cast<APawn>(OtherActor) || OtherActor->ActorHasTag(TEXT("Damageable")));
    if (!PhysicsComponent && !HitCharacter && !bDamageable)
        return;

    HitActorsThisAttack.Add(OtherActor);
    FVector Direction = ((PhysicsComponent ? PhysicsComponent->Bounds.Origin
        : OtherActor->GetActorLocation()) - GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero())
        Direction = GetActorForwardVector().GetSafeNormal2D();

    if (PhysicsComponent && AttackImpulse > 0.f)
    {
        FVector ImpactPoint = AttackHitbox->GetComponentLocation();
        if (bFromSweep && !SweepResult.ImpactPoint.IsNearlyZero())
            ImpactPoint = SweepResult.ImpactPoint;
        else
        {
            FVector ClosestPoint;
            if (PhysicsComponent->GetClosestPointOnCollision(ImpactPoint, ClosestPoint, PhysicsBone) >= 0.f)
                ImpactPoint = ClosestPoint;
        }
        PhysicsComponent->AddImpulseAtLocation(
            (Direction + FVector::UpVector * 0.15f).GetSafeNormal() * AttackImpulse,
            ImpactPoint, PhysicsBone);
    }
    else if (HitCharacter && AttackKnockbackSpeed > 0.f)
    {
        HitCharacter->LaunchCharacter(Direction * AttackKnockbackSpeed, false, false);
    }

    if (!bDamageable)
        return;
    const float AppliedDamage = UGameplayStatics::ApplyDamage(
        OtherActor, FMath::Max(0.f, AttackDamage), GetController(), this, UDamageType::StaticClass());
    if (AppliedDamage > 0.f)
    {
        if (auto* PC = Cast<ABasePlayerController>(GetController()))
        {
            const FVector Position = bFromSweep ? FVector(SweepResult.ImpactPoint)
                : OtherActor->GetActorLocation() + FVector(0.f, 0.f, 100.f);
            PC->ShowDamageNumber(AppliedDamage, Position);
        }
    }
}

float ABaseCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (Health <= 0.f || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
        return 0.f;
    const float AcceptedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    const float AppliedDamage = FMath::Clamp(AcceptedDamage, 0.f, Health);
    Health -= AppliedDamage;
    if (auto* PC = Cast<ABasePlayerController>(GetController()))
        PC->RefreshHealthUI();
    if (Health <= 0.f)
    {
        DisableAttackHitbox();
        GetCharacterMovement()->DisableMovement();
    }
    return AppliedDamage;
}
