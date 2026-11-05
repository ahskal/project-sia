
#include "BaseCharacter.h"
#include "BasePlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "CharacterAnimationComponent.h"
#include "Components/InventoryComponent.h"
#include "Data/HarvestDataTypes.h"
#include "Data/ItemDataStructs.h"
#include "Items/ItemBase.h"
#include "World/HarvestableResource.h"

#include "Camera/CameraComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
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
    HeldToolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldToolMesh"));
    HeldToolMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
    HeldToolMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeldToolMesh->SetGenerateOverlapEvents(false);
    HeldToolMesh->SetHiddenInGame(true);

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

    LeftAttackHitbox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftAttackHitbox"));
    LeftAttackHitbox->SetupAttachment(GetMesh(), TEXT("hand_l"));
    LeftAttackHitbox->SetRelativeLocation(FVector(8.f, 0.f, 0.f));
    LeftAttackHitbox->SetBoxExtent(FVector(12.f, 8.f, 8.f));
    LeftAttackHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftAttackHitbox->SetHiddenInGame(true);
    LeftAttackHitbox->SetGenerateOverlapEvents(true);
    LeftAttackHitbox->bDrawOnlyIfSelected = false;
    LeftAttackHitbox->ShapeColor = FColor::Red;
    LeftAttackHitbox->OnComponentBeginOverlap.AddDynamic(
        this, &ABaseCharacter::OnAttackHitboxOverlap);
    LeftAttackHitbox->SetCollisionObjectType(ECC_Pawn);
    LeftAttackHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    LeftAttackHitbox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
    LeftAttackHitbox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    LeftAttackHitbox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
    LeftAttackHitbox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

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
    GetCharacterMovement()->JumpZVelocity = JumpZVelocity;

    if (InventoryComponent)
    {
        InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &ABaseCharacter::OnEquipmentChanged);
        RefreshHeldToolVisual();
    }

}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (InventoryComponent)
        InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ABaseCharacter::OnEquipmentChanged);
    Super::EndPlay(EndPlayReason);
}

void ABaseCharacter::OnEquipmentChanged()
{
    RefreshHeldToolVisual();
}

EHarvestToolType ABaseCharacter::GetEquippedHarvestToolType() const
{
    const UItemBase* EquippedTool = InventoryComponent
        ? InventoryComponent->GetEquippedItem(EEquipmentSlot::Weapon) : nullptr;
    return EquippedTool ? EquippedTool->GetData().HarvestToolType : EHarvestToolType::None;
}

void ABaseCharacter::RefreshHeldToolVisual()
{
    if (!HeldToolMesh || !GetMesh())
        return;

    const UItemBase* EquippedTool = InventoryComponent
        ? InventoryComponent->GetEquippedItem(EEquipmentSlot::Weapon) : nullptr;
    const FItemData* ToolData = EquippedTool ? &EquippedTool->GetData() : nullptr;
    const bool bHasTool = ToolData && ToolData->HarvestToolType != EHarvestToolType::None;
    if (!bHasTool)
    {
        HeldToolMesh->SetStaticMesh(nullptr);
        HeldToolMesh->SetHiddenInGame(true);
        return;
    }

    const FName GripSocket = GetMesh()->DoesSocketExist(TEXT("HandGrip_R"))
        ? FName(TEXT("HandGrip_R")) : FName(TEXT("hand_r"));
    if (!GetMesh()->DoesSocketExist(GripSocket))
    {
        UE_LOG(LogTemp, Error, TEXT("Character '%s' cannot attach a harvesting tool: no right-hand grip socket or bone is available."),
            *GetName());
        HeldToolMesh->SetHiddenInGame(true);
        return;
    }

    UStaticMesh* ToolMesh = ToolData->AssetData.Mesh.LoadSynchronous();
    if (!ToolMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Equipped tool '%s' has no loadable mesh."), *EquippedTool->GetItemId().ToString());
        HeldToolMesh->SetHiddenInGame(true);
        return;
    }

    HeldToolMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale,
        GripSocket);
    HeldToolMesh->SetStaticMesh(ToolMesh);
    HeldToolMesh->SetRelativeTransform(ToolData->ToolGripTransform);
    HeldToolMesh->SetHiddenInGame(false);
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
    if (Health <= 0.f || !AnimationComponent)
        return;

    if (bIsAttack)
    {
        bAttackQueued = true;
        return;
    }
    if (!bCanAttack)
        return;

    StartAttack();
}

void ABaseCharacter::StartAttack()
{
    HitActorsThisAttack.Reset();
    bAttackQueued = false;
    bIsAttack = true;
    bCanAttack = false;

    const float AttackDuration = AnimationComponent->PlayComboAttack(CurrentComboStep);
    if (!FMath::IsFinite(AttackDuration) || AttackDuration <= 0.f)
    {
        UE_LOG(LogTemp, Error, TEXT("Character '%s' could not play combo attack %d."),
            *GetName(), CurrentComboStep + 1);
        bIsAttack = false;
        bCanAttack = true;
        CurrentComboStep = 0;
        return;
    }

    const int32 AttackCount = AnimationComponent->ComboAttacks.Num();
    CurrentComboStep = AttackCount > 0 ? (CurrentComboStep + 1) % AttackCount : 0;
    const float WindowStart = FMath::Clamp(AttackHitboxStartTime, 0.f, AttackDuration);
    const float WindowEnd = FMath::Clamp(WindowStart + FMath::Max(AttackHitboxDuration, 0.01f),
        WindowStart + 0.01f, AttackDuration);

    GetWorldTimerManager().SetTimer(AttackHitboxOpenHandle, this,
        &ABaseCharacter::EnableAttackHitbox, WindowStart, false);
    GetWorldTimerManager().SetTimer(AttackHitboxCloseHandle, this,
        &ABaseCharacter::DisableAttackHitbox, WindowEnd, false);
    GetWorldTimerManager().SetTimer(AttackFinishHandle, this,
        &ABaseCharacter::FinishAttack, AttackDuration, false);
    GetWorldTimerManager().SetTimer(AttackCooldownHandle, this,
        &ABaseCharacter::ResetAttack, FMath::Max(FMath::Max(AttackCooldown, 0.f), AttackDuration), false);
}

void ABaseCharacter::FinishAttack()
{
    DisableAttackHitbox();
    if (AnimationComponent)
        AnimationComponent->StopComboAttack();
    bIsAttack = false;

    if (bAttackQueued && Health > 0.f)
    {
        StartAttack();
        return;
    }

    bAttackQueued = false;
    CurrentComboStep = 0;
    if (!GetWorldTimerManager().IsTimerActive(AttackCooldownHandle))
        bCanAttack = true;
}

void ABaseCharacter::ResetAttack()
{
    if (bIsAttack)
        return;
    bCanAttack = true;
    DisableAttackHitbox();
}

// =======================================
// Attack Collision
// =======================================

void ABaseCharacter::EnableAttackHitbox()
{
	ActivateHitbox(AttackHitbox);
	ActivateHitbox(LeftAttackHitbox);
}

void ABaseCharacter::ActivateHitbox(UBoxComponent* Hitbox)
{
	if (!IsValid(Hitbox))
		return;

	Hitbox->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	Hitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hitbox->SetHiddenInGame(false);
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Attack Hitboxes: ON"));

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
		Results, Hitbox->GetComponentLocation(),
		Hitbox->GetComponentQuat(), ObjectTypes,
		FCollisionShape::MakeBox(Hitbox->GetScaledBoxExtent()), QueryParams);

    TSet<AActor*> ReportedActors;
    for (const FOverlapResult& Result : Results)
    {
        AActor* HitActor = Result.GetActor();
        if (IsValid(HitActor) && !ReportedActors.Contains(HitActor))
        {
            ReportedActors.Add(HitActor);
			OnAttackHitboxOverlap(Hitbox, HitActor,
                Result.GetComponent(), INDEX_NONE, false, FHitResult());
        }
    }
}

void ABaseCharacter::DisableAttackHitbox()
{
	for (UBoxComponent* Hitbox : { AttackHitbox.Get(), LeftAttackHitbox.Get() })
    {
		if (!IsValid(Hitbox))
			continue;
		Hitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hitbox->SetHiddenInGame(true);
    }
	if (GEngine)
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Yellow, TEXT("Attack Hitboxes: OFF"));
}

void ABaseCharacter::OnAttackHitboxOverlap(
    UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    UPrimitiveComponent* AttackSource = IsValid(OverlappedComponent)
        ? OverlappedComponent : AttackHitbox.Get();
    if (!bIsAttack || !IsValid(AttackSource)
        || AttackSource->GetCollisionEnabled() == ECollisionEnabled::NoCollision
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
                Component->Bounds.Origin, AttackSource->GetComponentLocation());
            if (Distance < NearestDistance)
            {
                NearestDistance = Distance;
                PhysicsComponent = Component;
            }
        }
    }

    ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
    AHarvestableResource* Resource = Cast<AHarvestableResource>(OtherActor);
    const bool bDamageable = OtherActor->CanBeDamaged()
        && (Cast<APawn>(OtherActor) || OtherActor->ActorHasTag(TEXT("Damageable")));
    if (!PhysicsComponent && !HitCharacter && !bDamageable && !Resource)
        return;

    HitActorsThisAttack.Add(OtherActor);
    FVector Direction = ((PhysicsComponent ? PhysicsComponent->Bounds.Origin
        : OtherActor->GetActorLocation()) - GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero())
        Direction = GetActorForwardVector().GetSafeNormal2D();

    if (Resource)
    {
        FHarvestHitContext Hit;
        Hit.BaseDamage = FMath::Max(0.f, AttackDamage);
        Hit.ToolType = GetEquippedHarvestToolType();
        Hit.AttackDirection = Direction;
        Hit.ImpactNormal = FVector::ZeroVector;
        Hit.Attacker = this;

        bool bHasImpactPoint = false;
        if (bFromSweep && (SweepResult.bBlockingHit || !SweepResult.ImpactPoint.IsNearlyZero()))
        {
            Hit.ImpactPoint = SweepResult.ImpactPoint;
            Hit.ImpactNormal = SweepResult.ImpactNormal.GetSafeNormal();
            bHasImpactPoint = true;
        }
        else if (IsValid(OtherComp))
        {
            FVector ClosestPoint = FVector::ZeroVector;
            if (OtherComp->GetClosestPointOnCollision(
                    AttackSource->GetComponentLocation(), ClosestPoint, NAME_None) >= 0.f)
            {
                Hit.ImpactPoint = ClosestPoint;
                bHasImpactPoint = true;
            }
            else
            {
                Hit.ImpactPoint = OtherComp->Bounds.GetBox().GetClosestPointTo(
                    AttackSource->GetComponentLocation());
                bHasImpactPoint = true;
            }
        }

        if (Hit.ImpactNormal.IsNearlyZero())
            Hit.ImpactNormal = bHasImpactPoint
                ? (Hit.ImpactPoint - OtherActor->GetActorLocation()).GetSafeNormal()
                : -Direction;
        if (Hit.ImpactNormal.IsNearlyZero())
            Hit.ImpactNormal = FVector::UpVector;

        const float AppliedDamage = Resource->ReceiveHarvestHit(Hit);
        if (AppliedDamage > 0.f)
        {
            if (auto* PC = Cast<ABasePlayerController>(GetController()))
                PC->ShowDamageNumber(AppliedDamage, bHasImpactPoint ? Hit.ImpactPoint : OtherActor->GetActorLocation());
        }
        return;
    }

    if (PhysicsComponent && AttackImpulse > 0.f)
    {
        FVector ImpactPoint = AttackSource->GetComponentLocation();
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
