#include "DamageTrap.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADamageTrap::ADamageTrap()
{
    PrimaryActorTick.bCanEverTick = false;
    SetCanBeDamaged(false);
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RedMaterial(
        TEXT("/Game/Traps/MI_DamageTrap.MI_DamageTrap"));

    Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
    Plate->SetupAttachment(RootComponent);
    Plate->SetStaticMesh(Cube.Object);
    Plate->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
    Plate->SetRelativeScale3D(FVector(2.f, 2.f, 0.08f));
    Plate->SetCollisionProfileName(TEXT("BlockAll"));
    if (RedMaterial.Succeeded()) Plate->SetMaterial(0, RedMaterial.Object);

    for (int32 Row = 0; Row < 3; ++Row)
    {
        for (int32 Column = 0; Column < 3; ++Column)
        {
            auto* Spike = CreateDefaultSubobject<UStaticMeshComponent>(
                *FString::Printf(TEXT("Spike_%d_%d"), Row, Column));
            Spike->SetupAttachment(RootComponent);
            Spike->SetStaticMesh(Cone.Object);
            Spike->SetRelativeLocation(FVector((Row - 1) * 60.f, (Column - 1) * 60.f, 23.f));
            Spike->SetRelativeScale3D(FVector(0.22f, 0.22f, 0.3f));
            Spike->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Spike->SetGenerateOverlapEvents(false);
        }
    }

    Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageTrigger"));
    Trigger->SetupAttachment(RootComponent);
    Trigger->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
    Trigger->SetBoxExtent(FVector(100.f, 100.f, 14.f));
    Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Trigger->SetCollisionObjectType(ECC_WorldDynamic);
    Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Trigger->SetGenerateOverlapEvents(true);
    Trigger->SetHiddenInGame(true);
    Trigger->OnComponentBeginOverlap.AddDynamic(this, &ADamageTrap::OnTrapEntered);
    Trigger->OnComponentEndOverlap.AddDynamic(this, &ADamageTrap::OnTrapExited);

    WarningText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("WarningText"));
    WarningText->SetupAttachment(RootComponent);
    WarningText->SetRelativeLocation(FVector(-112.f, 0.f, 42.f));
    WarningText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
    WarningText->SetHorizontalAlignment(EHTA_Center);
    WarningText->SetWorldSize(20.f);
    WarningText->SetTextRenderColor(FColor(255, 65, 65));
    WarningText->SetText(FText::FromString(TEXT("DAMAGE TRAP  -5 HP")));
    WarningText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADamageTrap::OnTrapEntered(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (!IsValid(Character) || OtherComp != Character->GetCapsuleComponent()
        || Occupants.Contains(Character))
        return;

    Occupants.Add(Character);
    UGameplayStatics::ApplyDamage(Character, 5.f, nullptr, this, UDamageType::StaticClass());
}

void ADamageTrap::OnTrapExited(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    const ACharacter* Character = Cast<ACharacter>(OtherActor);
    if (IsValid(Character) && OtherComp == Character->GetCapsuleComponent())
        Occupants.Remove(OtherActor);
}
