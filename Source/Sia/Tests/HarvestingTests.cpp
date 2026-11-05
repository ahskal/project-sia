#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Misc/ScopeExit.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "BaseCharacter.h"
#include "CharacterAnimationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/HarvestDataTypes.h"
#include "Data/ItemDataStructs.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollectionProxyData.h"
#include "Items/ItemBase.h"
#include "World/HarvestDebris.h"
#include "World/HarvestableResource.h"
#include "World/ItemPickup.h"
#include "World/RockResource.h"
#include "World/TreeResource.h"

namespace HarvestingTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	struct FScopedWorld
	{
		FScopedWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World)
				return;
			World->AddToRoot();
			if (GEngine)
			{
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
				bHasEngineContext = true;
			}
		}

		~FScopedWorld()
		{
			if (!World)
				return;
			World->EndPlay(EEndPlayReason::Destroyed);
			World->DestroyWorld(false);
			if (bHasEngineContext && GEngine)
				GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
		}

		void BeginPlay() const
		{
			if (World)
			{
				World->InitializeActorsForPlay(FURL());
				if (AWorldSettings* Settings = World->GetWorldSettings())
					Settings->NotifyBeginPlay();
			}
		}

		UWorld* World = nullptr;
		bool bHasEngineContext = false;
	};

	AActor* AddFloor(UWorld* World)
	{
		AActor* FloorActor = World->SpawnActor<AActor>();
		if (!FloorActor)
			return nullptr;
		UBoxComponent* Floor = NewObject<UBoxComponent>(FloorActor, TEXT("TestFloor"));
		FloorActor->SetRootComponent(Floor);
		Floor->SetBoxExtent(FVector(2000.f, 2000.f, 10.f));
		Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Floor->SetCollisionObjectType(ECC_WorldStatic);
		Floor->SetCollisionResponseToAllChannels(ECR_Block);
		Floor->RegisterComponent();
		FloorActor->SetActorLocation(FVector(0.f, 0.f, -10.f));
		return FloorActor;
	}

	UDataTable* ItemTable()
	{
		return LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/DT_Items.DT_Items"));
	}

	AItemPickup* FindPickup(UWorld* World, const FName ItemId)
	{
		for (TActorIterator<AItemPickup> It(World); It; ++It)
			if (It->ItemRow.RowName == ItemId)
				return *It;
		return nullptr;
	}

	int32 CountPickups(UWorld* World, const FName ItemId)
	{
		int32 Count = 0;
		for (TActorIterator<AItemPickup> It(World); It; ++It)
			if (It->ItemRow.RowName == ItemId)
				++Count;
		return Count;
	}

	UAnimSequenceBase* GetMontageSequence(UAnimInstance* AnimInstance)
	{
		UAnimMontage* Montage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
		if (!Montage)
			return nullptr;
		for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
			for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
				return Segment.GetAnimReference();
		return nullptr;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarvestingAssetDefinitionsTest,
	"Sia.Harvesting.AssetDefinitions", HarvestingTests::Flags)
bool FHarvestingAssetDefinitionsTest::RunTest(const FString&)
{
	UDataTable* Data = HarvestingTests::ItemTable();
	if (!TestNotNull(TEXT("production item DataTable"), Data))
		return false;
	TestEqual(TEXT("all original and harvesting rows are present"), Data->GetRowNames().Num(), 24);
	for (const FName RowName : { FName(TEXT("Axe")), FName(TEXT("Hammer")), FName(TEXT("Pickaxe")),
		FName(TEXT("Hoe")), FName(TEXT("Shovel")) })
	{
		const FItemData* Row = Data->FindRow<FItemData>(RowName, TEXT("Harvesting asset test"), false);
		if (!TestNotNull(FString::Printf(TEXT("%s tool row"), *RowName.ToString()), Row))
			return false;
		TestTrue(FString::Printf(TEXT("%s is a weapon tool"), *RowName.ToString()),
			Row->ItemType == EItemType::Weapon && Row->EquipmentSlot == EEquipmentSlot::Weapon
				&& Row->HarvestToolType != EHarvestToolType::None && Row->AssetData.Mesh.LoadSynchronous());
	}
	const FItemData* Pickaxe = Data->FindRow<FItemData>(TEXT("Pickaxe"), TEXT("Harvesting asset test"), false);
	const FItemData* Hoe = Data->FindRow<FItemData>(TEXT("Hoe"), TEXT("Harvesting asset test"), false);
	TestTrue(TEXT("Pickaxe row keeps the pickaxe name, type, and mesh"), Pickaxe
		&& Pickaxe->TextData.DisplayName.ToString() == TEXT("곡괭이")
		&& Pickaxe->HarvestToolType == EHarvestToolType::Pickaxe
		&& Pickaxe->AssetData.Mesh.GetAssetName() == TEXT("SM_Pickaxe"));
	TestTrue(TEXT("Hoe row keeps the hoe name, type, and mesh"), Hoe
		&& Hoe->TextData.DisplayName.ToString() == TEXT("괭이")
		&& Hoe->HarvestToolType == EHarvestToolType::Hoe
		&& Hoe->AssetData.Mesh.GetAssetName() == TEXT("SM_Hoe"));
	const FItemData* Log = Data->FindRow<FItemData>(TEXT("Log"), TEXT("Harvesting asset test"), false);
	const FItemData* Stone = Data->FindRow<FItemData>(TEXT("Stone"), TEXT("Harvesting asset test"), false);
	TestTrue(TEXT("Log uses SM_Log"), Log && Log->AssetData.Mesh.LoadSynchronous()
		&& Log->AssetData.Mesh.LoadSynchronous()->GetName() == TEXT("SM_Log"));
	TestTrue(TEXT("Stone pickup uses the largest extracted rock piece"), Stone && Stone->AssetData.Mesh.LoadSynchronous()
		&& Stone->AssetData.Mesh.LoadSynchronous()->GetName() == TEXT("SM_RockGroup01_Piece04"));

	UTreeHarvestProfile* TreeProfile = LoadObject<UTreeHarvestProfile>(nullptr,
		TEXT("/Game/Sia/Harvesting/Trees/Profiles/DA_TreeHarvestProfile.DA_TreeHarvestProfile"));
	URockHarvestProfile* RockProfile = LoadObject<URockHarvestProfile>(nullptr,
		TEXT("/Game/Sia/Harvesting/Rocks/Profiles/DA_RockHarvestProfile.DA_RockHarvestProfile"));
	if (!TestNotNull(TEXT("tree profile"), TreeProfile) || !TestNotNull(TEXT("rock profile"), RockProfile))
		return false;
	FText Error;
	TestTrue(TEXT("tree profile resolves all drop and presentation assets"), TreeProfile->IsValidForItemTable(Data, Error));
	TestTrue(TEXT("rock profile resolves all drop and fragment assets"), RockProfile->IsValidForItemTable(Data, Error));
	const TArray<FString> ExpectedTreeVariants{
		TEXT("SM_green-tree01"), TEXT("SM_green-tree02"), TEXT("SM_dead-tree03"),
		TEXT("SM_dead-tree04"), TEXT("SM_dead-tree05") };
	TestEqual(TEXT("tree profile has five STF variants"), TreeProfile->ResourceMeshVariants.Num(), 5);
	for (int32 Index = 0; Index < ExpectedTreeVariants.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("tree variant %d uses the requested STF mesh"), Index + 1),
			TreeProfile->ResourceMeshVariants.IsValidIndex(Index)
			&& TreeProfile->ResourceMeshVariants[Index]
			&& TreeProfile->ResourceMeshVariants[Index]->GetName() == ExpectedTreeVariants[Index]);
	}
	const TArray<FString> ExpectedRockVariants{
		TEXT("SM_RockPack_RockGroups_01_A"), TEXT("SM_RockPack_RockGroups_02_B"),
		TEXT("SM_RockPack_Rock_07") };
	const TArray<FString> ExpectedRockDestructionVariants{
		TEXT("GC_SM_RockPack_RockGroups_01_A"), TEXT("GC_SM_RockPack_RockGroups_02_B"),
		TEXT("GC_SM_RockPack_Rock_07") };
	TestEqual(TEXT("rock profile has the three requested main meshes"), RockProfile->ResourceMeshVariants.Num(), 3);
	for (int32 Index = 0; Index < ExpectedRockVariants.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("rock main variant %d uses the requested pack mesh"), Index + 1),
			RockProfile->ResourceMeshVariants.IsValidIndex(Index)
			&& RockProfile->ResourceMeshVariants[Index]
			&& RockProfile->ResourceMeshVariants[Index]->GetName() == ExpectedRockVariants[Index]);
		TestTrue(FString::Printf(TEXT("rock variant %d has its matching Chaos collection"), Index + 1),
			RockProfile->DestructionVariants.IsValidIndex(Index)
			&& RockProfile->DestructionVariants[Index]
			&& RockProfile->DestructionVariants[Index]->GetName() == ExpectedRockDestructionVariants[Index]);
	}
	TestTrue(TEXT("rock collision height is extended and cleanup is three seconds"),
		FMath::IsNearlyEqual(RockProfile->CollisionVerticalScale, 1.4f)
		&& FMath::IsNearlyEqual(RockProfile->CleanupDelaySeconds, 3.f));
	TestTrue(TEXT("tree trunk collision is narrow enough to approach"),
		FMath::IsNearlyEqual(TreeProfile->TrunkCollisionRadius, 35.f)
		&& FMath::IsNearlyEqual(TreeProfile->TrunkCollisionHeight, 320.f));
	TestEqual(TEXT("rock fragments are configured in four outside-in stages"), RockProfile->Pieces.Num(), 4);
	const FHarvestDropRule* LogRule = TreeProfile->Drops.FindByPredicate(
		[](const FHarvestDropRule& Rule) { return Rule.ItemId == TEXT("Log"); });
	const FHarvestDropRule* AppleRule = TreeProfile->Drops.FindByPredicate(
		[](const FHarvestDropRule& Rule) { return Rule.ItemId == TEXT("Apple"); });
	const FHarvestDropRule* StoneRule = RockProfile->Drops.FindByPredicate(
		[](const FHarvestDropRule& Rule) { return Rule.ItemId == TEXT("Stone"); });
	TestTrue(TEXT("tree logs have a guaranteed configurable yield of two to four"),
		LogRule && LogRule->Probability == 1.f && LogRule->MinAmount == 2 && LogRule->MaxAmount == 4);
	TestTrue(TEXT("tree apples have the configured rare yield"),
		AppleRule && FMath::IsNearlyEqual(AppleRule->Probability, .18f)
			&& AppleRule->MinAmount == 1 && AppleRule->MaxAmount == 2);
	TestTrue(TEXT("rocks have a guaranteed configurable yield of two to four"),
		StoneRule && StoneRule->Probability == 1.f && StoneRule->MinAmount == 2 && StoneRule->MaxAmount == 4);
	for (int32 Index = 0; Index < RockProfile->Pieces.Num(); ++Index)
	{
		const FString ExpectedName = FString::Printf(TEXT("SM_RockGroup01_Piece%02d"), Index + 1);
		TestTrue(FString::Printf(TEXT("rock stage %d follows the authored outside-in order"), Index + 1),
			RockProfile->Pieces[Index].Mesh && RockProfile->Pieces[Index].Mesh->GetName() == ExpectedName);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHarvestingRulesTest,
	"Sia.Harvesting.ToolDamageAndSizeYields", HarvestingTests::Flags)
bool FHarvestingRulesTest::RunTest(const FString&)
{
	TestEqual(TEXT("axe bonus applies to trees"),
		SiaHarvesting::ResolveDamage(10.f, EHarvestToolType::Axe, EHarvestToolType::Axe, 1.5f), 15.f);
	TestEqual(TEXT("axe bonus does not apply to rocks"),
		SiaHarvesting::ResolveDamage(10.f, EHarvestToolType::Axe, EHarvestToolType::Pickaxe, 1.5f), 10.f);

	FHarvestDropRule Logs;
	Logs.ItemId = TEXT("Log");
	Logs.Probability = 1.f;
	Logs.MinAmount = 2;
	Logs.MaxAmount = 2;
	FHarvestDropRule BonusLogs = Logs;
	BonusLogs.MinAmount = BonusLogs.MaxAmount = 3;
	TArray<FHarvestDropRule> Rules{ Logs, BonusLogs };
	FRandomStream Random(31415);
	const TArray<FHarvestResolvedDrop> Drops = SiaHarvesting::ResolveDrops(Rules, 2.f, 1.f, Random);
	TestEqual(TEXT("duplicate item rules merge to one pickup"), Drops.Num(), 1);
	if (!Drops.IsEmpty())
	{
		TestEqual(TEXT("resource size scales total yield"), Drops[0].Quantity, 10);
		TestEqual(TEXT("merged drop retains its item ID"), Drops[0].ItemId, FName(TEXT("Log")));
	}
	FHarvestDropRule DisabledDrop = Logs;
	DisabledDrop.Probability = 0.f;
	TestTrue(TEXT("zero probability suppresses the configured item"),
		SiaHarvesting::ResolveDrops({ DisabledDrop }, 1.f, 1.f, Random).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTreeHarvestGameplayTest,
	"Sia.Harvesting.TreeFallsDropsAndPaysOnce", HarvestingTests::Flags)
bool FTreeHarvestGameplayTest::RunTest(const FString&)
{
	HarvestingTests::FScopedWorld TestWorld;
	if (!TestNotNull(TEXT("game world"), TestWorld.World))
		return false;
	HarvestingTests::AddFloor(TestWorld.World);
	TestWorld.BeginPlay();
	UClass* TreeClass = LoadClass<ATreeResource>(nullptr,
		TEXT("/Game/Sia/Harvesting/Trees/BP_TreeResource.BP_TreeResource_C"));
	if (!TestNotNull(TEXT("tree Blueprint"), TreeClass))
		return false;
	ATreeResource* Tree = TestWorld.World->SpawnActor<ATreeResource>(TreeClass, FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("tree actor"), Tree))
		return false;
	TestTrue(TEXT("tree begins play with its profile"), Tree->HasActorBegunPlay());
	TestEqual(TEXT("tree profile is active in play"), Tree->GetMaximumHealth(), 100.f);
	TestEqual(TEXT("tree health initializes at play"), Tree->GetRemainingHealth(), 100.f);
	UTreeHarvestProfile* TreeProfile = LoadObject<UTreeHarvestProfile>(nullptr,
		TEXT("/Game/Sia/Harvesting/Trees/Profiles/DA_TreeHarvestProfile.DA_TreeHarvestProfile"));
	UStaticMeshComponent* TreeMesh = Tree->FindComponentByClass<UStaticMeshComponent>();
	UBoxComponent* TreeCollision = Tree->FindComponentByClass<UBoxComponent>();
	TestTrue(TEXT("tree selects one of its five profile variants"), TreeProfile && TreeMesh
		&& TreeMesh->GetStaticMesh()
		&& TreeProfile->ResourceMeshVariants.ContainsByPredicate([TreeMesh](const TObjectPtr<UStaticMesh>& Mesh)
			{ return Mesh == TreeMesh->GetStaticMesh(); }));
	TestTrue(TEXT("tree collision is a trunk-sized overlap, not a canopy blocker"), TreeCollision
		&& TreeCollision->GetUnscaledBoxExtent().X <= 35.f
		&& TreeCollision->GetUnscaledBoxExtent().Y <= 35.f
		&& TreeCollision->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Overlap);
	const FHarvestHitContext Hit{ 100.f, EHarvestToolType::Axe, FVector(0.f, 0.f, 60.f),
		FVector::UpVector, FVector::ForwardVector, nullptr };
	TestEqual(TEXT("lethal hit applies remaining health"), Tree->ReceiveHarvestHit(Hit), 100.f);
	TestTrue(TEXT("tree enters depleted and falling state"), Tree->IsDepleted());
	TestEqual(TEXT("one guaranteed log pickup appears"), HarvestingTests::CountPickups(TestWorld.World, TEXT("Log")), 1);
	AItemPickup* Log = HarvestingTests::FindPickup(TestWorld.World, TEXT("Log"));
	if (TestNotNull(TEXT("log pickup"), Log))
	{
		TestTrue(TEXT("configured log quantity is honored"), Log->Quantity >= 2 && Log->Quantity <= 4);
		UStaticMeshComponent* Mesh = Log->FindComponentByClass<UStaticMeshComponent>();
		if (TestNotNull(TEXT("log pickup mesh"), Mesh))
		{
			Mesh->UpdateBounds();
			TestTrue(TEXT("harvested log rests on the test floor"), FMath::IsNearlyEqual(Mesh->Bounds.GetBox().Min.Z, 0.f, 1.f));
		}
	}
	int32 DebrisCount = 0;
	for (TActorIterator<AHarvestDebris> It(TestWorld.World); It; ++It)
		++DebrisCount;
	TestEqual(TEXT("one tree hit creates localized chip debris"), DebrisCount, 2);
	TestEqual(TEXT("a depleted tree rejects another hit"), Tree->ReceiveHarvestHit(Hit), 0.f);
	TestEqual(TEXT("a depleted tree cannot pay twice"), HarvestingTests::CountPickups(TestWorld.World, TEXT("Log")), 1);
	const FRotator StandingRotation = Tree->GetActorRotation();
	Tree->Tick(.7f);
	TestFalse(TEXT("tree visibly starts to fall after depletion"), Tree->GetActorRotation().Equals(StandingRotation, .1f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRockHarvestGameplayTest,
	"Sia.Harvesting.RockBreaksOutsideInAndDropsStone", HarvestingTests::Flags)
bool FRockHarvestGameplayTest::RunTest(const FString&)
{
	TSharedPtr<HarvestingTests::FScopedWorld> TestWorld = MakeShared<HarvestingTests::FScopedWorld>();
	if (!TestNotNull(TEXT("game world"), TestWorld->World))
		return false;
	HarvestingTests::AddFloor(TestWorld->World);
	TestWorld->BeginPlay();
	UClass* RockClass = LoadClass<ARockResource>(nullptr,
		TEXT("/Game/Sia/Harvesting/Rocks/BP_RockResource.BP_RockResource_C"));
	URockHarvestProfile* Profile = LoadObject<URockHarvestProfile>(nullptr,
		TEXT("/Game/Sia/Harvesting/Rocks/Profiles/DA_RockHarvestProfile.DA_RockHarvestProfile"));
	if (!TestNotNull(TEXT("rock Blueprint"), RockClass) || !TestNotNull(TEXT("rock profile"), Profile))
		return false;
	ARockResource* CenterHitRock = TestWorld->World->SpawnActor<ARockResource>(RockClass, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator);
	ARockResource* EdgeHitRock = TestWorld->World->SpawnActor<ARockResource>(RockClass, FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator);
	ARockResource* BrokenRock = TestWorld->World->SpawnActor<ARockResource>(RockClass, FVector(1000.f, 0.f, 0.f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("center-hit rock"), CenterHitRock)
		|| !TestNotNull(TEXT("edge-hit rock"), EdgeHitRock)
		|| !TestNotNull(TEXT("depleted rock"), BrokenRock))
		return false;
	TestTrue(TEXT("rock begins play with its profile"), BrokenRock->HasActorBegunPlay());
	TestEqual(TEXT("rock profile is active in play"), BrokenRock->GetMaximumHealth(), 80.f);
	TestEqual(TEXT("rock health initializes at play"), BrokenRock->GetRemainingHealth(), 80.f);
	bool bHasMainRockMesh = false;
	for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(BrokenRock))
		bHasMainRockMesh |= Mesh && Mesh->IsVisible()
			&& Profile->ResourceMeshVariants.ContainsByPredicate([Mesh](const TObjectPtr<UStaticMesh>& Variant)
				{ return Variant == Mesh->GetStaticMesh(); });
	TestTrue(TEXT("rock displays one of the three requested main meshes"), bHasMainRockMesh);
	UBoxComponent* RockCollision = BrokenRock->FindComponentByClass<UBoxComponent>();
	UStaticMeshComponent* MainRockMesh = nullptr;
	for (UStaticMeshComponent* Mesh : TInlineComponentArray<UStaticMeshComponent*>(BrokenRock))
		if (Mesh && Mesh->GetFName() == TEXT("ResourceMesh"))
			MainRockMesh = Mesh;
	TestTrue(TEXT("rock collision uses the mesh footprint and extends 1.4 times vertically"), RockCollision
		&& MainRockMesh && MainRockMesh->GetStaticMesh()
		&& FMath::IsNearlyEqual(RockCollision->GetUnscaledBoxExtent().X,
			MainRockMesh->GetStaticMesh()->GetBounds().BoxExtent.X, 1.f)
		&& FMath::IsNearlyEqual(RockCollision->GetUnscaledBoxExtent().Y,
			MainRockMesh->GetStaticMesh()->GetBounds().BoxExtent.Y, 1.f)
		&& FMath::IsNearlyEqual(RockCollision->GetUnscaledBoxExtent().Z,
			MainRockMesh->GetStaticMesh()->GetBounds().BoxExtent.Z * Profile->CollisionVerticalScale, 1.f));
	UGeometryCollectionComponent* Destruction = BrokenRock->FindComponentByClass<UGeometryCollectionComponent>();
	TestTrue(TEXT("each rock is backed by a pre-fractured collection"), Destruction
		&& Destruction->GetRestCollection()
		&& Profile->DestructionVariants.Contains(Destruction->GetRestCollection())
		&& Destruction->GetNumElements(FName(TEXT("Transform"))) > 1);
	CenterHitRock->ReceiveHarvestHit({ 1.f, EHarvestToolType::None, FVector(0.f, 0.f, 20.f), FVector::UpVector, FVector::ForwardVector, nullptr });
	EdgeHitRock->ReceiveHarvestHit({ 1.f, EHarvestToolType::None, FVector(400.f, 0.f, 20.f), FVector::UpVector, FVector::ForwardVector, nullptr });
	const FHarvestRockPiece& FirstPiece = Profile->Pieces[0];
	int32 FirstPieceDebrisCount = 0;
	for (TActorIterator<AHarvestDebris> It(TestWorld->World); It; ++It)
	{
		const UStaticMeshComponent* Mesh = It->FindComponentByClass<UStaticMeshComponent>();
		if (Mesh && Mesh->GetStaticMesh() == FirstPiece.Mesh)
			++FirstPieceDebrisCount;
	}
	TestEqual(TEXT("center and edge impacts release the same outer fragment"), FirstPieceDebrisCount, 2);
	const FHarvestHitContext LethalHit{ 80.f, EHarvestToolType::Pickaxe, FVector(1000.f, 0.f, 20.f),
		FVector::UpVector, FVector::ForwardVector, nullptr };
	BrokenRock->ReceiveHarvestHit(LethalHit);
	TestTrue(TEXT("rock reaches depleted state"), BrokenRock->IsDepleted());
	TestEqual(TEXT("broken rock produces one Stone pickup"), HarvestingTests::CountPickups(TestWorld->World, TEXT("Stone")), 1);
	AItemPickup* Stone = HarvestingTests::FindPickup(TestWorld->World, TEXT("Stone"));
	TestTrue(TEXT("stone quantity follows the profile range"), Stone && Stone->Quantity >= 2 && Stone->Quantity <= 4);
	if (Stone)
	{
		UStaticMeshComponent* Mesh = Stone->FindComponentByClass<UStaticMeshComponent>();
		if (TestNotNull(TEXT("stone pickup mesh"), Mesh))
		{
			Mesh->UpdateBounds();
			TestTrue(TEXT("harvested stone rests on the test floor"),
				FMath::IsNearlyEqual(Mesh->Bounds.GetBox().Min.Z, 0.f, 1.f));
		}
	}
	TestTrue(TEXT("the standing rock is hidden while Chaos debris is active"), MainRockMesh
		&& !MainRockMesh->IsVisible() && Destruction && Destruction->IsVisible()
		&& Destruction->IsSimulatingPhysics() && BrokenRock->GetActorEnableCollision());
	TestTrue(TEXT("the rock schedules cleanup after exactly three seconds"),
		FMath::IsNearlyEqual(BrokenRock->GetLifeSpan(), 3.f));

	const TWeakObjectPtr<ARockResource> WeakRock(BrokenRock);
	const TWeakObjectPtr<UGeometryCollectionComponent> WeakDestruction(Destruction);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
		[this, TestWorld, WeakRock, WeakDestruction, Elapsed = 0.f, bObservedFracture = false]() mutable
		{
			if (!TestWorld->World)
			{
				TestFalse(TEXT("rock fracture world remains available"), true);
				return true;
			}
			const float DeltaSeconds = .05f;
			TestWorld->World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			Elapsed += DeltaSeconds;
			if (WeakDestruction.IsValid())
			{
				if (FGeometryDynamicCollection* DynamicCollection = WeakDestruction->GetDynamicCollection())
				{
					const FGeometryCollectionDynamicStateFacade DynamicState(*DynamicCollection);
					if (DynamicState.IsValid())
					{
						for (int32 Index = 0; Index < WeakDestruction->GetNumElements(FName(TEXT("Transform"))); ++Index)
							bObservedFracture |= DynamicState.HasBrokenOff(Index);
					}
				}
			}
			if (Elapsed >= 3.1f)
			{
				TestTrue(TEXT("Chaos reports at least one fragment detached from its cluster"), bObservedFracture);
				TestFalse(TEXT("the depleted rock disappears after the cleanup delay"), WeakRock.IsValid());
				return true;
			}
			return false;
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterComboAndDualHitboxTest,
	"Sia.Combat.QueuedComboAndBothHandHitboxes", HarvestingTests::Flags)
bool FCharacterComboAndDualHitboxTest::RunTest(const FString&)
{
	TSharedPtr<HarvestingTests::FScopedWorld> TestWorld = MakeShared<HarvestingTests::FScopedWorld>();
	if (!TestNotNull(TEXT("game world"), TestWorld->World))
		return false;
	TestWorld->BeginPlay();
	ABaseCharacter* Character = TestWorld->World->SpawnActor<ABaseCharacter>();
	if (!TestNotNull(TEXT("character"), Character))
		return false;
	UBoxComponent* RightHitbox = Character->FindComponentByClass<UBoxComponent>();
	UBoxComponent* LeftHitbox = nullptr;
	TInlineComponentArray<UBoxComponent*> Boxes(Character);
	for (UBoxComponent* Box : Boxes)
	{
		if (Box && Box->GetFName() == TEXT("LeftAttackHitbox"))
			LeftHitbox = Box;
		if (Box && Box->GetFName() == TEXT("AttackHitbox"))
			RightHitbox = Box;
	}
	if (!TestNotNull(TEXT("right-hand hitbox"), RightHitbox) || !TestNotNull(TEXT("left-hand hitbox"), LeftHitbox))
		return false;
	TestEqual(TEXT("right hitbox follows right hand"), RightHitbox->GetAttachSocketName(), FName(TEXT("AttackSocket")));
	TestEqual(TEXT("left hitbox follows left hand"), LeftHitbox->GetAttachSocketName(), FName(TEXT("hand_l")));
	Character->EnableAttackHitbox();
	TestEqual(TEXT("right collision is enabled"), RightHitbox->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("left collision is enabled"), LeftHitbox->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	Character->DisableAttackHitbox();
	TestEqual(TEXT("both collisions close together"), LeftHitbox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	UCharacterAnimationComponent* Animation = Character->FindComponentByClass<UCharacterAnimationComponent>();
	if (!TestNotNull(TEXT("character animation instance"), AnimInstance)
		|| !TestNotNull(TEXT("character animation component"), Animation))
		return false;
	UInventoryComponent* Inventory = Character->GetInventory();
	if (!TestTrue(TEXT("production item table initializes character inventory"),
		Inventory && Inventory->InitializeInventory(HarvestingTests::ItemTable())))
		return false;
	TestEqual(TEXT("axe enters inventory"), Inventory->TryAddItem(TEXT("Axe"), 1).AmountAdded, 1);
	UItemBase* EquippedAxe = Inventory->GetEquippedItem(EEquipmentSlot::Weapon);
	TestTrue(TEXT("axe occupies the existing weapon slot"),
		EquippedAxe && EquippedAxe->GetItemId() == FName(TEXT("Axe")));
	UStaticMeshComponent* HeldTool = Character->FindComponentByClass<UStaticMeshComponent>();
	if (TestNotNull(TEXT("held tool visual"), HeldTool))
	{
		TestFalse(TEXT("equipped axe is visible"), HeldTool->bHiddenInGame);
		TestEqual(TEXT("equipped axe attaches to the authored right-hand grip"), HeldTool->GetAttachSocketName(), FName(TEXT("HandGrip_R")));
		if (TestNotNull(TEXT("equipped axe mesh"), HeldTool->GetStaticMesh().Get()))
			TestEqual(TEXT("equipped axe uses the extracted axe mesh"), HeldTool->GetStaticMesh()->GetName(), FString(TEXT("SM_Axe")));
		const FVector AxeLongAxis = HeldTool->GetComponentTransform()
			.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
		TestTrue(TEXT("equipped axe handle points upward beside the character"),
			FVector::DotProduct(AxeLongAxis, FVector::UpVector) > .9f);
	}
	UClass* TreeClass = LoadClass<ATreeResource>(nullptr,
		TEXT("/Game/Sia/Harvesting/Trees/BP_TreeResource.BP_TreeResource_C"));
	if (!TestNotNull(TEXT("tree Blueprint"), TreeClass))
		return false;
	const FVector LeftHandLocation = LeftHitbox->GetComponentLocation();
	RightHitbox->SetWorldLocation(LeftHandLocation + FVector(10000.f, 0.f, 0.f));
	ATreeResource* Tree = TestWorld->World->SpawnActor<ATreeResource>(TreeClass, LeftHandLocation, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("tree target"), Tree))
		return false;
	if (UBoxComponent* ResourceCollision = Tree->FindComponentByClass<UBoxComponent>())
		Tree->SetActorLocation(LeftHandLocation - ResourceCollision->GetRelativeLocation());
	TestEqual(TEXT("tree starts with full health"), Tree->GetRemainingHealth(), 100.f);
	Character->Attack();
	Character->EnableAttackHitbox();
	TestEqual(TEXT("left-hand attack uses the equipped axe bonus"), Tree->GetRemainingHealth(), 70.f);
	Character->DisableAttackHitbox();
	UAnimSequenceBase* FirstAttack = HarvestingTests::GetMontageSequence(AnimInstance);
	if (!TestNotNull(TEXT("first attack montage"), FirstAttack))
		return false;
	TestEqual(TEXT("first input plays MM_Attack_01"), FirstAttack->GetName(), FString(TEXT("MM_Attack_01")));
	Character->Attack();
	const TWeakObjectPtr<ABaseCharacter> WeakCharacter(Character);
	const TWeakObjectPtr<UAnimInstance> WeakAnimInstance(AnimInstance);
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(
		[this, TestWorld, WeakCharacter, WeakAnimInstance, Elapsed = 0.f]() mutable
		{
			if (!WeakCharacter.IsValid() || !WeakAnimInstance.IsValid() || !TestWorld->World)
			{
				TestFalse(TEXT("combo test world remains available while attack timers run"), true);
				return true;
			}
			const float DeltaSeconds = FMath::Clamp(FApp::GetDeltaTime(), .001f, .05f);
			TestWorld->World->GetTimerManager().Tick(DeltaSeconds);
			WeakAnimInstance->UpdateAnimation(DeltaSeconds, false);
			Elapsed += DeltaSeconds;
			if (Elapsed < 1.1f)
				return false;

			UAnimSequenceBase* SecondAttack = HarvestingTests::GetMontageSequence(WeakAnimInstance.Get());
			if (!TestNotNull(TEXT("queued attack starts after the first"), SecondAttack))
				return true;
			TestEqual(TEXT("queued attack alternates to MM_Attack_02"),
				SecondAttack->GetName(), FString(TEXT("MM_Attack_02")));
			return true;
		}));
	return true;
}

#endif
