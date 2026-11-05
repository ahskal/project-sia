#include "World/RockResource.h"

#include "Components/StaticMeshComponent.h"
#include "Data/HarvestDataTypes.h"
#include "Engine/StaticMesh.h"

ARockResource::ARockResource()
{
	ResourceMesh->SetVisibility(false, true);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool ARockResource::ConfigureResourcePresentation(FText& OutError)
{
	const URockHarvestProfile* RockProfile = Cast<URockHarvestProfile>(Profile);
	if (!RockProfile || !Super::ConfigureResourcePresentation(OutError))
	{
		OutError = FText::FromString(TEXT("바위 액터에 바위 수확 프로필이 지정되지 않았습니다."));
		return false;
	}
	ResourceMesh->SetVisibility(false, true);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PieceComponents.Reset();
	NextPieceIndex = 0;
	FBox CollisionBounds(ForceInit);

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
		PieceTransform.SetLocation(PieceTransform.GetLocation() * ResourceSize);
		PieceTransform.SetScale3D(PieceTransform.GetScale3D() * ResourceSize);
		PieceComponent->SetRelativeTransform(PieceTransform);
		PieceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PieceComponent->RegisterComponent();
		PieceComponents.Add(PieceComponent);

		const FBoxSphereBounds MeshBounds = Piece.Mesh->GetBounds();
		const FVector PieceCenter = Piece.RelativeTransform.TransformPosition(MeshBounds.Origin);
		const FVector PieceExtent = MeshBounds.BoxExtent * Piece.RelativeTransform.GetScale3D();
		CollisionBounds += FBox::BuildAABB(PieceCenter, PieceExtent);
	}
	if (CollisionBounds.IsValid)
		SetResourceCollisionBounds(CollisionBounds.GetCenter(), CollisionBounds.GetExtent());
	return !PieceComponents.IsEmpty();
}

void ARockResource::OnAcceptedHit(const FHarvestHitContext&, const bool)
{
	const URockHarvestProfile* RockProfile = Cast<URockHarvestProfile>(Profile);
	if (RockProfile)
		ReleaseNextPieces(RockProfile->PiecesReleasedPerHit);
}

void ARockResource::OnResourceDepleted(const FHarvestHitContext& Hit)
{
	ReleaseNextPieces(PieceComponents.Num());
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
