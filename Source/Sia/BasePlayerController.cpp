
#include "BasePlayerController.h"
#include "CombatHUDWidget.h"
#include "BaseCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

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

    MoveAction = MoveAsset.Object;
    LookAction = LookAsset.Object;
    MouseLookAction = MouseLookAsset.Object;
    JumpAction = JumpAsset.Object;
    DashAction = DashAsset.Object;
    AttackAction = AttackAsset.Object;
    InventoryAction = InventoryAsset.Object;
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
    RefreshHealthUI();
}

void ABasePlayerController::OnUnPossess()
{
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
        InventoryUI->SetItems(InventoryView);
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

        InventoryUI->SetItems(InventoryView);

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
