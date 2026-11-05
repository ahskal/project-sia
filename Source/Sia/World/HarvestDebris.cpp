#include "World/HarvestDebris.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AHarvestDebris::AHarvestDebris()
{
	PrimaryActorTick.bCanEverTick = false;
	PhysicsBody = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsBody"));
	SetRootComponent(PhysicsBody);
	PhysicsBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsBody->SetCollisionObjectType(ECC_PhysicsBody);
	PhysicsBody->SetCollisionResponseToAllChannels(ECR_Block);
	PhysicsBody->SetSimulatePhysics(false);

	MeshVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshVisual"));
	MeshVisual->SetupAttachment(PhysicsBody);
	MeshVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshVisual->SetGenerateOverlapEvents(false);
}

void AHarvestDebris::InitializeDebris(UStaticMesh* Mesh, const FVector& MeshScale,
	const FVector& Impulse, const float LifeSpanSeconds)
{
	const bool bFiniteScale = FMath::IsFinite(MeshScale.X)
		&& FMath::IsFinite(MeshScale.Y) && FMath::IsFinite(MeshScale.Z);
	if (!IsValid(Mesh) || !bFiniteScale || MeshScale.GetMin() <= 0.f)
	{
		Destroy();
		return;
	}

	MeshVisual->SetStaticMesh(Mesh);
	MeshVisual->SetRelativeScale3D(MeshScale);
	const FVector BoxExtent = Mesh->GetBounds().BoxExtent * MeshScale;
	PhysicsBody->SetBoxExtent(FVector(FMath::Max(1.f, BoxExtent.X),
		FMath::Max(1.f, BoxExtent.Y), FMath::Max(1.f, BoxExtent.Z)) * 0.65f, false);
	PhysicsBody->SetSimulatePhysics(true);
	PhysicsBody->AddImpulse(Impulse, NAME_None, true);
	SetLifeSpan(FMath::Max(LifeSpanSeconds, 0.1f));
}
