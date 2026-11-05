#include "World/HarvestableResource.h"

#include "BaseCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ItemDataStructs.h"
#include "Engine/DataTable.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Items/ItemBase.h"
#include "World/HarvestDebris.h"
#include "World/ItemPickup.h"

namespace
{
	UDataTable* LoadHarvestItemTable()
	{
		return LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Items.DT_Items"));
	}
}

AHarvestableResource::AHarvestableResource()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HarvestRoot"));
	SetRootComponent(SceneRoot);
	ResourceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh"));
	ResourceMesh->SetupAttachment(SceneRoot);
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ResourceCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("ResourceCollision"));
	ResourceCollision->SetupAttachment(SceneRoot);
	ResourceCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ResourceCollision->SetCollisionObjectType(ECC_WorldDynamic);
	ResourceCollision->SetCollisionResponseToAllChannels(ECR_Block);
	ResourceCollision->SetGenerateOverlapEvents(true);
}

void AHarvestableResource::BeginPlay()
{
	Super::BeginPlay();

	UDataTable* ItemTable = LoadHarvestItemTable();
	FText Error;
	bool bValidConfiguration = true;
	if (!Profile)
	{
		Error = FText::FromString(TEXT("수확 프로필이 지정되지 않았습니다."));
		bValidConfiguration = false;
	}
	else if (!FMath::IsFinite(ResourceSize) || ResourceSize <= 0.f)
	{
		Error = FText::FromString(TEXT("수확 자원의 크기는 0보다 큰 유한한 값이어야 합니다."));
		bValidConfiguration = false;
	}
	else if (!Profile->IsValidForItemTable(ItemTable, Error)
		|| !ConfigureResourcePresentation(Error))
	{
		bValidConfiguration = false;
	}

	if (!bValidConfiguration)
	{
		UE_LOG(LogTemp, Error, TEXT("Harvest resource '%s' is disabled: %s"),
			*GetName(), *Error.ToString());
		SetCanBeDamaged(false);
		SetActorEnableCollision(false);
		return;
	}

	RemainingHealth = Profile->MaxHealth;
	bDepleted = false;
	bProfileReady = true;
	DropRandom.Initialize(FMath::Rand());
	SetCanBeDamaged(true);
}

bool AHarvestableResource::ConfigureResourcePresentation(FText& OutError)
{
	if (!ResourceMesh)
	{
		OutError = FText::FromString(TEXT("수확 대상 메시 컴포넌트가 없습니다."));
		return false;
	}
	ResourceMesh->SetRelativeScale3D(FVector(ResourceSize));
	ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ResourceCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OutError = FText::GetEmpty();
	return true;
}

void AHarvestableResource::SetResourceCollisionBounds(const FVector& Center, const FVector& Extent)
{
	const bool bFinite = FMath::IsFinite(Center.X) && FMath::IsFinite(Center.Y)
		&& FMath::IsFinite(Center.Z) && FMath::IsFinite(Extent.X)
		&& FMath::IsFinite(Extent.Y) && FMath::IsFinite(Extent.Z);
	if (!ResourceCollision || !bFinite)
		return;
	ResourceCollision->SetRelativeLocation(Center * ResourceSize);
	ResourceCollision->SetBoxExtent(FVector(
		FMath::Max(1.f, Extent.X * ResourceSize),
		FMath::Max(1.f, Extent.Y * ResourceSize),
		FMath::Max(1.f, Extent.Z * ResourceSize)), false);
}

void AHarvestableResource::OnAcceptedHit(const FHarvestHitContext&, bool)
{
}

void AHarvestableResource::OnResourceDepleted(const FHarvestHitContext&)
{
	SetLifeSpan(1.f);
}

float AHarvestableResource::TakeDamage(const float DamageAmount, const FDamageEvent& DamageEvent,
	AController*, AActor* DamageCauser)
{
	FHarvestHitContext Hit;
	Hit.BaseDamage = DamageAmount;
	Hit.Attacker = DamageCauser;
	if (IsValid(DamageCauser))
		Hit.AttackDirection = DamageCauser->GetActorForwardVector();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamage = static_cast<const FPointDamageEvent*>(&DamageEvent);
		Hit.ImpactPoint = PointDamage->HitInfo.ImpactPoint;
		Hit.ImpactNormal = PointDamage->HitInfo.ImpactNormal;
	}
	return ReceiveHarvestHit(Hit);
}

float AHarvestableResource::ReceiveHarvestHit(const FHarvestHitContext& Hit)
{
	if (!bProfileReady || bDepleted || !Profile || !FMath::IsFinite(Hit.BaseDamage)
		|| Hit.BaseDamage <= 0.f)
		return 0.f;

	const float Damage = SiaHarvesting::ResolveDamage(Hit.BaseDamage, Hit.ToolType,
		Profile->PreferredTool, Profile->PreferredToolDamageMultiplier);
	if (Damage <= 0.f)
		return 0.f;

	const float AppliedDamage = FMath::Min(RemainingHealth, Damage);
	RemainingHealth = FMath::Max(0.f, RemainingHealth - AppliedDamage);
	const bool bLethal = RemainingHealth <= 0.f;
	if (bLethal)
	{
		// Close the payout gate before fracture effects, delegates, or pickup spawning.
		bDepleted = true;
		DisableResourceCollision();
	}

	OnAcceptedHit(Hit, bLethal);
	if (bLethal)
	{
		OnResourceDepleted(Hit);
		SpawnResolvedDrops(GetItemDataTable(Hit));
	}
	return AppliedDamage;
}

void AHarvestableResource::DisableResourceCollision()
{
	SetCanBeDamaged(false);
	SetActorEnableCollision(false);
	if (ResourceMesh)
		ResourceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ResourceCollision)
		ResourceCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UDataTable* AHarvestableResource::GetItemDataTable(const FHarvestHitContext& Hit) const
{
	if (ABaseCharacter* Attacker = Cast<ABaseCharacter>(Hit.Attacker.Get()))
	{
		if (UInventoryComponent* Inventory = Attacker->GetInventory())
		{
			if (UDataTable* ItemTable = Inventory->GetItemDataTable())
				return ItemTable;
		}
	}
	return LoadHarvestItemTable();
}

void AHarvestableResource::SpawnResolvedDrops(UDataTable* ItemTable)
{
	if (!ItemTable || !Profile || !GetWorld())
		return;

	const TArray<FHarvestResolvedDrop> Drops = SiaHarvesting::ResolveDrops(
		Profile->Drops, ResourceSize, Profile->SizeYieldExponent, DropRandom);
	int32 DropIndex = 0;
	for (const FHarvestResolvedDrop& Drop : Drops)
	{
		if (Drop.Quantity <= 0 || !ItemTable->FindRow<FItemData>(Drop.ItemId, TEXT("HarvestDrop"), false))
			continue;

		const FVector SpawnLocation = GetActorLocation()
			+ FVector(0.f, 0.f, 30.f + 12.f * DropIndex++);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AItemPickup* Pickup = GetWorld()->SpawnActor<AItemPickup>(SpawnLocation, FRotator::ZeroRotator, Params);
		if (!Pickup)
		{
			UE_LOG(LogTemp, Error, TEXT("Harvest drop spawn failed for '%s'; resource remains depleted."),
				*Drop.ItemId.ToString());
			continue;
		}

		Pickup->ItemRow.DataTable = ItemTable;
		Pickup->ItemRow.RowName = Drop.ItemId;
		Pickup->Quantity = Drop.Quantity;
		Pickup->RefreshForInventoryDrop();
	}
}

void AHarvestableResource::SpawnDebris(UStaticMesh* Mesh, const FVector& Location,
	const FRotator& Rotation, const FVector& Scale, const FVector& Impulse, const float LifeSpanSeconds)
{
	if (!IsValid(Mesh) || !GetWorld())
		return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AHarvestDebris* Debris = GetWorld()->SpawnActor<AHarvestDebris>(Location, Rotation, Params);
	if (Debris)
		Debris->InitializeDebris(Mesh, Scale, Impulse, LifeSpanSeconds);
}
