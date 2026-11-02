
#include "BasePlayerController.h"
#include "CombatHUDWidget.h"
#include "BaseCharacter.h"
#include "Components/InventoryComponent.h"
#include "World/ItemPickup.h"
#include "EngineUtils.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

ABasePlayerController::ABasePlayerController()
{
    static ConstructorHelpers::FClassFinder<UCombatHUDWidget> HUDAsset(
        TEXT("/Game/UI/WBP_HealthHUD"));

    HealthHUDClass = HUDAsset.Class;

    InventoryWidgetClass = TSoftClassPtr<UInventoryWidget>(
        FSoftObjectPath(
            TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C")
        )
    );

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> DefaultContextAsset(
        TEXT("/Game/Input/IMC_Default.IMC_Default"));

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> MouseContextAsset(
        TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));

    static ConstructorHelpers::FObjectFinder<UInputAction> MoveAsset(
        TEXT("/Game/Input/Actions/IA_Move.IA_Move"));

    static ConstructorHelpers::FObjectFinder<UInputAction> LookAsset(
        TEXT("/Game/Input/Actions/IA_Look.IA_Look"));

    static ConstructorHelpers::FObjectFinder<UInputAction> MouseLookAsset(
        TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));

    static ConstructorHelpers::FObjectFinder<UInputAction> JumpAsset(
        TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));

    static ConstructorHelpers::FObjectFinder<UInputAction> DashAsset(
        TEXT("/Game/Input/Actions/IA_Dash.IA_Dash"));

    static ConstructorHelpers::FObjectFinder<UInputAction> AttackAsset(
        TEXT("/Game/Input/Actions/IA_Attack.IA_Attack"));

    static ConstructorHelpers::FObjectFinder<UInputAction> InventoryAsset(
        TEXT("/Game/Input/Actions/IA_Inventory.IA_Inventory"));

    DefaultMappingContext = DefaultContextAsset.Object;
    MouseLookMappingContext = MouseContextAsset.Object;

    QuickSlotMappingContext = NewObject<UInputMappingContext>(this, TEXT("QuickSlotMappingContext"));
    QuickSlotActions.SetNum(4);
    const FKey QuickSlotKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const FString ActionPath = FString::Printf(TEXT("/Game/Input/Actions/IA_Quick%d.IA_Quick%d"), Index + 1, Index + 1);
        ConstructorHelpers::FObjectFinder<UInputAction> ActionAsset(*ActionPath);
        UInputAction* Action = ActionAsset.Object;
        QuickSlotActions[Index] = Action;
        if (!Action) continue;
        const bool bMapped = DefaultMappingContext && DefaultMappingContext->GetMappings().ContainsByPredicate(
            [Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action; });
        if (!bMapped) QuickSlotMappingContext->MapKey(Action, QuickSlotKeys[Index]);
    }

    MoveAction = MoveAsset.Object;
    LookAction = LookAsset.Object;
    MouseLookAction = MouseLookAsset.Object;
    JumpAction = JumpAsset.Object;
    DashAction = DashAsset.Object;
    AttackAction = AttackAsset.Object;
    InventoryAction = InventoryAsset.Object;
    static ConstructorHelpers::FObjectFinder<UInputAction> InteractAsset(
        TEXT("/Game/Input/Actions/IA_Interaction.IA_Interaction"));
    InteractAction = InteractAsset.Object;
}

void ABasePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
        return;

    CreateCombatHUD();

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            GetLocalPlayer());

    if (!Subsystem)
        return;

    if (DefaultMappingContext)
    {
        Subsystem->AddMappingContext(DefaultMappingContext, 0);
    }

    if (MouseLookMappingContext)
    {
        Subsystem->AddMappingContext(MouseLookMappingContext, 1);
    }
    if (QuickSlotMappingContext)
        Subsystem->AddMappingContext(QuickSlotMappingContext, 2);
}

void ABasePlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput =
        Cast<UEnhancedInputComponent>(InputComponent);

    if (!EnhancedInput)
    {
        UE_LOG(LogTemp, Error,
            TEXT("Enhanced Input Component Missing"));
        return;
    }

    if (QuickSlotActions.Num() == 4)
    {
        void (ABasePlayerController::*Handlers[])() = {
            &ABasePlayerController::UseQuickSlot1, &ABasePlayerController::UseQuickSlot2,
            &ABasePlayerController::UseQuickSlot3, &ABasePlayerController::UseQuickSlot4
        };
        for (int32 Index = 0; Index < 4; ++Index)
            if (QuickSlotActions[Index])
                EnhancedInput->BindAction(QuickSlotActions[Index], ETriggerEvent::Started, this, Handlers[Index]);
    }

    if (MoveAction)
    {
        EnhancedInput->BindAction(
            MoveAction,
            ETriggerEvent::Triggered,
            this,
            &ABasePlayerController::Move);

        EnhancedInput->BindAction(
            MoveAction,
            ETriggerEvent::Completed,
            this,
            &ABasePlayerController::Move);
    }

    if (LookAction)
    {
        EnhancedInput->BindAction(
            LookAction,
            ETriggerEvent::Triggered,
            this,
            &ABasePlayerController::Look);
    }

    if (MouseLookAction)
    {
        EnhancedInput->BindAction(
            MouseLookAction,
            ETriggerEvent::Triggered,
            this,
            &ABasePlayerController::Look);
    }

    if (JumpAction)
    {
        EnhancedInput->BindAction(
            JumpAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::StartJump);

        EnhancedInput->BindAction(
            JumpAction,
            ETriggerEvent::Completed,
            this,
            &ABasePlayerController::StopJump);
    }

    if (DashAction)
    {
        EnhancedInput->BindAction(
            DashAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::Dash);
    }

    if (AttackAction)
    {
        EnhancedInput->BindAction(
            AttackAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::Attack);
    }

    if (InventoryAction)
    {
        EnhancedInput->BindAction(
            InventoryAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::ToggleInventory);
    }
    if (InteractAction)
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ABasePlayerController::Interact);
}

void ABasePlayerController::Move(const FInputActionValue& Value)
{
    if (bInventoryOpen)
        return;

    ABaseCharacter* PlayerCharacter =
        Cast<ABaseCharacter>(GetPawn());

    if (!IsValid(PlayerCharacter))
        return;

    PlayerCharacter->MoveCharacter(Value.Get<FVector2D>());
}

void ABasePlayerController::Look(const FInputActionValue& Value)
{
    if (bInventoryOpen)
        return;

    const FVector2D Input = Value.Get<FVector2D>();

    AddYawInput(Input.X);
    AddPitchInput(Input.Y);
}

void ABasePlayerController::StartJump()
{
    if (bInventoryOpen)
        return;

    ABaseCharacter* PlayerCharacter =
        Cast<ABaseCharacter>(GetPawn());

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->Jump();
    }
}

void ABasePlayerController::StopJump()
{
    ABaseCharacter* PlayerCharacter =
        Cast<ABaseCharacter>(GetPawn());

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->StopJumping();
    }
}

void ABasePlayerController::Dash()
{
    if (bInventoryOpen)
        return;

    ABaseCharacter* PlayerCharacter =
        Cast<ABaseCharacter>(GetPawn());

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->Dash();
    }
}

void ABasePlayerController::Attack()
{
    if (bInventoryOpen)
        return;

    ABaseCharacter* PlayerCharacter =
        Cast<ABaseCharacter>(GetPawn());

    if (!IsValid(PlayerCharacter))
        return;

    PlayerCharacter->Attack();
}

void ABasePlayerController::CreateCombatHUD()
{
    if (!IsLocalController() || CombatHUD || !HealthHUDClass)
        return;

    CombatHUD = CreateWidget<UCombatHUDWidget>(
        this, HealthHUDClass);

    if (CombatHUD)
    {
        CombatHUD->AddToPlayerScreen();
        RefreshHealthUI();
    }
}

void ABasePlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    CreateCombatHUD();
    const auto* PlayerCharacter = Cast<ABaseCharacter>(InPawn);
    UInventoryComponent* Inventory = PlayerCharacter ? PlayerCharacter->GetInventory() : nullptr;
    if (CombatHUD)
        CombatHUD->BindInventory(Inventory);
    RefreshHealthUI();
    if (InventoryUI)
        InventoryUI->BindInventory(Inventory);
}

void ABasePlayerController::UseQuickSlot1() { UseQuickSlot(0); }
void ABasePlayerController::UseQuickSlot2() { UseQuickSlot(1); }
void ABasePlayerController::UseQuickSlot3() { UseQuickSlot(2); }
void ABasePlayerController::UseQuickSlot4() { UseQuickSlot(3); }

void ABasePlayerController::UseQuickSlot(int32 Index)
{
    auto* PlayerCharacter = Cast<ABaseCharacter>(GetPawn());
    if (!PlayerCharacter || !PlayerCharacter->GetInventory()) return;
    FText Message;
    PlayerCharacter->GetInventory()->UseQuickSlot(Index, Message);
    if (InventoryUI) InventoryUI->ShowInventoryMessage(Message);
    if (!Message.IsEmpty()) ClientMessage(Message.ToString());
}

void ABasePlayerController::OnUnPossess()
{
    if (InventoryUI) InventoryUI->BindInventory(nullptr);
    Super::OnUnPossess();
    RefreshHealthUI();
}

void ABasePlayerController::RefreshHealthUI()
{
    if (!CombatHUD)
        return;

    const ABaseCharacter* PlayerCharacter =
        Cast<ABaseCharacter>(GetPawn());

    CombatHUD->SetHealth(
        PlayerCharacter ? PlayerCharacter->GetHealth() : 0.f,
        PlayerCharacter ? PlayerCharacter->GetMaxHealth() : 100.f);
}

void ABasePlayerController::ShowDamageNumber(
    float Damage,
    const FVector& WorldPosition)
{
    if (CombatHUD)
    {
        CombatHUD->ShowDamage(Damage, WorldPosition);
    }
}

void ABasePlayerController::SetInventoryItems(
    const TArray<FInventoryItemView>& Items)
{
    InventoryView = Items;

    if (IsValid(InventoryUI))
    {
        if (!InventoryUI->GetBoundInventory()) InventoryUI->SetItems(InventoryView);
    }
}

void ABasePlayerController::ToggleInventory()
{
    if (!IsLocalController())
        return;

    UE_LOG(LogTemp, Warning, TEXT("Inventory Input Called"));

    if (!IsValid(InventoryUI))
    {
        // 블루프린트 클래스 로드
        UClass* WidgetClass = InventoryWidgetClass.LoadSynchronous();

        if (!WidgetClass)
        {
            UE_LOG(LogTemp, Error,
                TEXT("Inventory Widget Load Failed. Path: %s"),
                *InventoryWidgetClass.ToSoftObjectPath().ToString());

            return;
        }

        // C++ 부모 클래스 확인
        if (!WidgetClass->IsChildOf(UInventoryWidget::StaticClass()))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Inventory Widget Parent Class Invalid"));

            return;
        }

        // 인벤토리 위젯 생성
        InventoryUI = CreateWidget<UInventoryWidget>(
            this,
            WidgetClass
        );

        if (!IsValid(InventoryUI))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Inventory Widget Create Failed"));

            return;
        }

        if (auto* PlayerCharacter = Cast<ABaseCharacter>(GetPawn()))
            InventoryUI->BindInventory(PlayerCharacter->GetInventory());
        else InventoryUI->SetItems(InventoryView);

        UE_LOG(LogTemp, Warning,
            TEXT("Inventory Widget Created: %s"),
            *InventoryUI->GetClass()->GetName());
    }

    bInventoryOpen = !bInventoryOpen;

    SetIgnoreMoveInput(bInventoryOpen);
    SetIgnoreLookInput(bInventoryOpen);

    bShowMouseCursor = bInventoryOpen;

    if (bInventoryOpen)
    {
        // 화면에 추가
        if (!InventoryUI->IsInViewport())
        {
            InventoryUI->AddToPlayerScreen(20);
        }

        // UI 표시
        InventoryUI->SetVisibility(
            ESlateVisibility::Visible
        );

        FInputModeGameAndUI Mode;
        Mode.SetWidgetToFocus(InventoryUI->TakeWidget());
        Mode.SetHideCursorDuringCapture(false);

        SetInputMode(Mode);
        InventoryUI->SetKeyboardFocus();

        UE_LOG(LogTemp, Warning,
            TEXT("Inventory Open"));
    }
    else
    {
        if (InventoryUI) InventoryUI->CloseItemContextMenu();
        // UI 숨김
        InventoryUI->SetVisibility(
            ESlateVisibility::Collapsed
        );

        SetInputMode(FInputModeGameOnly());

        UE_LOG(LogTemp, Warning,
            TEXT("Inventory Closed"));
    }
}

void ABasePlayerController::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(InventoryUI))
    {
        InventoryUI->RemoveFromParent();
    }

    InventoryUI = nullptr;

    if (IsValid(CombatHUD))
    {
        CombatHUD->RemoveFromParent();
    }

    CombatHUD = nullptr;

    Super::EndPlay(EndPlayReason);
}

void ABasePlayerController::Interact()
{
    if (bInventoryOpen) return;
    auto* PlayerCharacter = Cast<ABaseCharacter>(GetPawn());
    if (!PlayerCharacter || !GetWorld()) return;
    AItemPickup* Nearest = nullptr;
    double BestDistance = TNumericLimits<double>::Max();
    for (TActorIterator<AItemPickup> It(GetWorld()); It; ++It)
    {
        if (It->IsActorBeingDestroyed()) continue;
        const double Distance = FVector::DistSquared(It->GetActorLocation(), PlayerCharacter->GetActorLocation());
        if (Distance <= FMath::Square(It->GetPickupRange()) && Distance < BestDistance)
        {
            Nearest = *It;
            BestDistance = Distance;
        }
    }
    if (Nearest) Nearest->TryPickup(PlayerCharacter);
}

void ABasePlayerController::GiveItem(FName ItemId, int32 Quantity)
{
    auto* PlayerCharacter = Cast<ABaseCharacter>(GetPawn());
    if (!PlayerCharacter || !PlayerCharacter->GetInventory()) return;
    const FItemAddResult Result = PlayerCharacter->GetInventory()->TryAddItem(ItemId, Quantity);
    UE_LOG(LogTemp, Display, TEXT("GiveItem: %s | %d / %d"), *Result.Message.ToString(), Result.AmountAdded, Quantity);
}
