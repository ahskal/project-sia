#include "World/RockResource.h"

#include "Components/StaticMeshComponent.h"
#include "Data/HarvestDataTypes.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"

ARockResource::ARockResource()
{
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DestructionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("DestructionComponent"));
	DestructionComponent->SetupAttachment(SceneRoot);
	DestructionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DestructionComponent->SetVisibility(false, true);
}

bool ARockResource::ConfigureResourcePresentation(FText& OutError)
{
	const URockHarvestProfile* RockProfile = Cast<URockHarvestProfile>(Profile);
	if (!RockProfile || !Super::ConfigureResourcePresentation(OutError))
	{
		OutError = FText::FromString(TEXT("바위 액터에 바위 수확 프로필이 지정되지 않았습니다."));
		return false;
	}
	if (RockProfile->ResourceMeshVariants.IsEmpty()
		|| RockProfile->DestructionVariants.Num() != RockProfile->ResourceMeshVariants.Num())
	{
		OutError = FText::FromString(TEXT("바위 메시와 Geometry Collection 바리에이션 수가 일치하지 않습니다."));
		return false;
	}

	SelectedVariationIndex = FMath::RandRange(0, RockProfile->ResourceMeshVariants.Num() - 1);
	if (!RockProfile->ResourceMeshVariants.IsValidIndex(SelectedVariationIndex)
		|| !RockProfile->DestructionVariants.IsValidIndex(SelectedVariationIndex))
	{
		OutError = FText::FromString(TEXT("바위 메시와 Geometry Collection 바리에이션을 선택하지 못했습니다."));
		return false;
	}
	UStaticMesh* StandingMesh = RockProfile->ResourceMeshVariants[SelectedVariationIndex];
	UGeometryCollection* DestructionCollection = RockProfile->DestructionVariants[SelectedVariationIndex];
	if (!StandingMesh)
	{
		OutError = FText::FromString(TEXT("바위 수확 프로필에서 사용할 메시를 선택하지 못했습니다."));
		return false;
	}
	if (!DestructionCollection || !DestructionComponent)
	{
		OutError = FText::FromString(TEXT("선택된 바위 메시의 Chaos Geometry Collection이 없습니다."));
		return false;
	}
	ResourceMesh->SetStaticMesh(StandingMesh);
	ResourceMesh->SetVisibility(true, true);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DestructionComponent->SetRestCollection(DestructionCollection);
	DestructionComponent->SetRelativeTransform(ResourceMesh->GetRelativeTransform());
	DestructionComponent->SetVisibility(false, true);
	DestructionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PieceComponents.Reset();
	NextPieceIndex = 0;
	const FBoxSphereBounds MainBounds = StandingMesh->GetBounds();
	FVector CollisionExtent = MainBounds.BoxExtent;
	CollisionExtent.Z *= RockProfile->CollisionVerticalScale;
	SetResourceCollisionBounds(MainBounds.Origin, CollisionExtent);

	for (int32 Index = 0; Index < RockProfile->Pieces.Num(); ++Index)
	{
		const FHarvestRockPiece& Piece = RockProfile->Pieces[Index];
		UStaticMeshComponent* PieceComponent = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("RockPiece_%02d"), Index + 1));
		if (!PieceComponent)
			continue;
		PieceComponent->SetupAttachment(SceneRoot);
		PieceComponent->SetStaticMesh(Piece.Mesh);
		FTransform PieceTransform = Piece.RelativeTransform;
		FVector Outward = Piece.RelativeTransform.GetLocation().GetSafeNormal();
		if (Outward.IsNearlyZero())
			Outward = FVector::UpVector;
		const float LargestAxis = FMath::Max(FMath::Abs(Outward.X),
			FMath::Max(FMath::Abs(Outward.Y), FMath::Abs(Outward.Z)));
		Outward /= LargestAxis;
		const FVector SurfaceOffset = FVector(
			Outward.X * MainBounds.BoxExtent.X,
			Outward.Y * MainBounds.BoxExtent.Y,
			Outward.Z * MainBounds.BoxExtent.Z) * .8f;
		PieceTransform.SetLocation((MainBounds.Origin + SurfaceOffset) * ResourceSize);
		PieceTransform.SetScale3D(PieceTransform.GetScale3D() * ResourceSize);
		PieceComponent->SetRelativeTransform(PieceTransform);
		PieceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PieceComponent->SetVisibility(false, true);
		PieceComponent->RegisterComponent();
		PieceComponents.Add(PieceComponent);
	}
	return ResourceMesh->GetStaticMesh() && DestructionComponent->GetRestCollection()
		&& !PieceComponents.IsEmpty();
}

void ARockResource::OnAcceptedHit(const FHarvestHitContext&, const bool bWasLethal)
{
	if (bWasLethal)
		return;

	const URockHarvestProfile* RockProfile = Cast<URockHarvestProfile>(Profile);
	if (RockProfile)
		ReleaseNextPieces(RockProfile->PiecesReleasedPerHit);
}

void ARockResource::OnResourceDepleted(const FHarvestHitContext& Hit)
{
	ResourceMesh->SetVisibility(false, true);
	// Re-enable actor-level collision so the Geometry Collection can collide with the world.
	SetActorEnableCollision(true);
	if (DestructionComponent)
	{
		DestructionComponent->SetRelativeTransform(ResourceMesh->GetRelativeTransform());
		DestructionComponent->SetCollisionObjectType(ECC_PhysicsBody);
		DestructionComponent->SetCollisionResponseToAllChannels(ECR_Block);
		DestructionComponent->SetEnableGravity(true);
		DestructionComponent->SetVisibility(true, true);
		DestructionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DestructionComponent->SetSimulatePhysics(true);
		DestructionComponent->CrumbleActiveClusters();
	}
	const URockHarvestProfile* RockProfile = Cast<URockHarvestProfile>(Profile);
	SetLifeSpan(RockProfile ? RockProfile->CleanupDelaySeconds : 1.f);
	(void)Hit;
}

void ARockResource::ReleaseNextPieces(const int32 Count)
{
	const URockHarvestProfile* RockProfile = Cast<URockHarvestProfile>(Profile);
	if (!RockProfile)
		return;

	const int32 EndIndex = FMath::Min(NextPieceIndex + FMath::Max(Count, 0), PieceComponents.Num());
	for (; NextPieceIndex < EndIndex; ++NextPieceIndex)
	{
		UStaticMeshComponent* PieceComponent = PieceComponents[NextPieceIndex];
		if (!IsValid(PieceComponent))
			continue;

		const FVector Location = PieceComponent->GetComponentLocation();
		const FRotator Rotation = PieceComponent->GetComponentRotation();
		const FVector Scale = PieceComponent->GetComponentScale();
		const FVector Outward = (Location - GetActorLocation()).GetSafeNormal();
		const FVector Direction = (Outward + FVector::UpVector * .25f).GetSafeNormal();
		PieceComponent->SetVisibility(false, true);
		PieceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SpawnDebris(PieceComponent->GetStaticMesh(), Location, Rotation, Scale,
			Direction * RockProfile->PieceImpulse, RockProfile->CleanupDelaySeconds);
	}
}
