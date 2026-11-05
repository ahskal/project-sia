#include "World/TreeResource.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HarvestDataTypes.h"
#include "Engine/StaticMesh.h"

ATreeResource::ATreeResource()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ATreeResource::BeginPlay()
{
	Super::BeginPlay();
	FallStartRotation = GetActorRotation();
}

bool ATreeResource::ConfigureResourcePresentation(FText& OutError)
{
	const UTreeHarvestProfile* TreeProfile = Cast<UTreeHarvestProfile>(Profile);
	if (!TreeProfile || !Super::ConfigureResourcePresentation(OutError))
	{
		OutError = FText::FromString(TEXT("나무 액터에 나무 수확 프로필이 지정되지 않았습니다."));
		return false;
	}
	const int32 VariationIndex = FMath::RandRange(0, TreeProfile->ResourceMeshVariants.Num() - 1);
	UStaticMesh* StandingMesh = TreeProfile->ResourceMeshVariants[VariationIndex];
	if (!StandingMesh)
	{
		OutError = FText::FromString(TEXT("나무 수확 프로필에서 사용할 메시를 선택하지 못했습니다."));
		return false;
	}
	ResourceMesh->SetStaticMesh(StandingMesh);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResourceCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	const float HalfHeight = TreeProfile->TrunkCollisionHeight * .5f;
	SetResourceCollisionBounds(FVector(0.f, 0.f, HalfHeight),
		FVector(TreeProfile->TrunkCollisionRadius, TreeProfile->TrunkCollisionRadius, HalfHeight));
	return true;
}

void ATreeResource::OnAcceptedHit(const FHarvestHitContext& Hit, const bool)
{
	const UTreeHarvestProfile* TreeProfile = Cast<UTreeHarvestProfile>(Profile);
	if (!TreeProfile)
		return;

	const FVector AttackDirection = Hit.AttackDirection.GetSafeNormal2D();
	FVector SideDirection = FVector::CrossProduct(FVector::UpVector, AttackDirection).GetSafeNormal();
	if (SideDirection.IsNearlyZero())
		SideDirection = GetActorRightVector();

	for (int32 Index = 0; Index < TreeProfile->ChipsPerHit; ++Index)
	{
		const float Side = FMath::FRandRange(-TreeProfile->ChipRadius, TreeProfile->ChipRadius);
		const float Height = FMath::FRandRange(-TreeProfile->ChipRadius * .5f, TreeProfile->ChipRadius * .5f);
		const FVector Location = Hit.ImpactPoint + SideDirection * Side + FVector::UpVector * Height;
		FVector Impulse = (Hit.ImpactNormal + AttackDirection * .35f + FVector::UpVector * .45f).GetSafeNormal();
		Impulse *= TreeProfile->ChipImpulse * FMath::FRandRange(.8f, 1.2f);
		const float Scale = TreeProfile->ChipScale * FMath::FRandRange(.85f, 1.15f);
		SpawnDebris(TreeProfile->HitChipMesh, Location, FRotator::ZeroRotator,
			FVector(Scale), Impulse, 1.5f);
	}
}

void ATreeResource::OnResourceDepleted(const FHarvestHitContext& Hit)
{
	const UTreeHarvestProfile* TreeProfile = Cast<UTreeHarvestProfile>(Profile);
	if (!TreeProfile)
	{
		Super::OnResourceDepleted(Hit);
		return;
	}

	FallStartRotation = GetActorRotation();
	FVector FallDirection = Hit.AttackDirection.GetSafeNormal2D();
	if (FallDirection.IsNearlyZero())
		FallDirection = GetActorForwardVector();
	const FVector LocalFall = GetActorTransform().InverseTransformVectorNoScale(FallDirection).GetSafeNormal2D();
	const float Pitch = LocalFall.X * TreeProfile->FallAngleDegrees;
	const float Roll = -LocalFall.Y * TreeProfile->FallAngleDegrees;
	FallTargetRotation = FallStartRotation + FRotator(Pitch, 0.f, Roll);
	FallElapsed = 0.f;
	bFalling = true;
	SetActorTickEnabled(true);
}

void ATreeResource::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bFalling)
		return;

	const UTreeHarvestProfile* TreeProfile = Cast<UTreeHarvestProfile>(Profile);
	if (!TreeProfile)
		return;
	FallElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(FallElapsed / TreeProfile->FallDuration, 0.f, 1.f);
	const float EasedAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
	SetActorRotation(FMath::Lerp(FallStartRotation, FallTargetRotation, EasedAlpha));
	if (Alpha >= 1.f)
	{
		bFalling = false;
		SetActorTickEnabled(false);
		SetLifeSpan(TreeProfile->CleanupDelaySeconds);
	}
}
