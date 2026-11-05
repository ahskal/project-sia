#include "World/ItemPickup.h"
#include "BaseCharacter.h"
#include "Components/InventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/ItemDataStructs.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AItemPickup::AItemPickup()
{
    PrimaryActorTick.bCanEverTick = false;
    auto* Root = CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot"));
    SetRootComponent(Root);
    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    ItemMesh->SetupAttachment(Root);
    ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    ItemMesh->SetStaticMesh(Sphere.Object);
    ItemMesh->SetRelativeScale3D(FVector(.3f));
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PickupLabel"));
    Label->SetupAttachment(Root);
    Label->SetRelativeLocation(FVector(0, 0, 45));
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetWorldSize(16.f);
    Label->SetTextRenderColor(FColor(230, 195, 125));
    ItemRow.RowName = TEXT("Apple");
}

void AItemPickup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshVisuals();
}

void AItemPickup::BeginPlay()
{
    Super::BeginPlay();
    RefreshVisuals();
}

void AItemPickup::RefreshVisuals()
{
    if (!ItemRow.DataTable)
        ItemRow.DataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Items.DT_Items"));
    const FItemData* Row = ItemRow.DataTable && ItemRow.DataTable->GetRowStruct() == FItemData::StaticStruct()
        ? ItemRow.GetRow<FItemData>(TEXT("Pickup")) : nullptr;
    UStaticMesh* Mesh = Row ? Row->AssetData.Mesh.LoadSynchronous() : nullptr;
    ItemMesh->SetStaticMesh(Mesh ? Mesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    ItemMesh->SetRelativeScale3D(Mesh ? FVector(1.f) : FVector(.3f));
    Label->SetText(FText::FromString(FString::Printf(TEXT("%s x %d\n[F] Pick up"), *ItemRow.RowName.ToString(), Quantity)));
}

void AItemPickup::RefreshForInventoryDrop()
{
    RefreshVisuals();
    PlaceOnGround();
}

void AItemPickup::PlaceOnGround()
{
    UWorld* World = GetWorld();
    if (!World || !ItemMesh) return;

    const FVector TraceStart = GetActorLocation() + FVector(0.f, 0.f, 100.f);
    const FVector TraceEnd = GetActorLocation() - FVector(0.f, 0.f, 300.f);
    FCollisionObjectQueryParams ObjectTypes;
    ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ItemPickupGroundPlacement), false, this);
    TArray<FHitResult> Hits;
    World->LineTraceMultiByObjectType(Hits, TraceStart, TraceEnd, ObjectTypes, QueryParams);

    const FHitResult* Ground = Hits.FindByPredicate([](const FHitResult& Hit)
    {
        return Hit.ImpactNormal.Z >= 0.5f;
    });
    if (!Ground) return;

    ItemMesh->UpdateBounds();
    const float MeshBottomOffset = ItemMesh->GetStaticMesh()
        ? ItemMesh->Bounds.GetBox().Min.Z - GetActorLocation().Z
        : 0.f;
    FVector GroundedLocation = GetActorLocation();
    GroundedLocation.Z = Ground->ImpactPoint.Z - MeshBottomOffset;
    SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

int32 AItemPickup::TryPickup(ABaseCharacter* Character)
{
    if (!IsValid(Character) || Quantity <= 0 || IsActorBeingDestroyed()
        || FVector::DistSquared(GetActorLocation(), Character->GetActorLocation()) > FMath::Square(PickupRange)) return 0;
    UInventoryComponent* Inventory = Character->GetInventory();
    UDataTable* InventoryTable = Inventory ? Inventory->GetItemDataTable() : nullptr;
    if (!InventoryTable || ItemRow.RowName.IsNone()
        || !InventoryTable->FindRow<FItemData>(ItemRow.RowName, TEXT("Pickup"), false))
    {
        UE_LOG(LogTemp, Warning, TEXT("Pickup failed: item '%s' is not in the character inventory table."),
            *ItemRow.RowName.ToString());
        return 0;
    }
    const FItemAddResult Result = Inventory->TryAddItem(ItemRow.RowName, Quantity);
    UE_LOG(LogTemp, Log, TEXT("Pickup: %s"), *Result.Message.ToString());
    Quantity -= Result.AmountAdded;
    if (Quantity <= 0) Destroy();
    else RefreshVisuals();
    return Result.AmountAdded;
}
