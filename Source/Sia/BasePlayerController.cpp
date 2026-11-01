#include "BasePlayerController.h"
#include "BaseCharacter.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

void ABasePlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
        return;

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
            GetLocalPlayer()
        );

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
        return;

    if (MoveAction)
    {
        EnhancedInput->BindAction(
            MoveAction,
            ETriggerEvent::Triggered,
            this,
            &ABasePlayerController::Move
        );

        EnhancedInput->BindAction(
            MoveAction,
            ETriggerEvent::Completed,
            this,
            &ABasePlayerController::Move
        );
    }

    if (LookAction)
    {
        EnhancedInput->BindAction(
            LookAction,
            ETriggerEvent::Triggered,
            this,
            &ABasePlayerController::Look
        );
    }

    if (JumpAction)
    {
        EnhancedInput->BindAction(
            JumpAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::StartJump
        );

        EnhancedInput->BindAction(
            JumpAction,
            ETriggerEvent::Completed,
            this,
            &ABasePlayerController::StopJump
        );
    }

    if (DashAction)
    {
        EnhancedInput->BindAction(
            DashAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::Dash
        );
    }

    if (AttackAction)
    {
        EnhancedInput->BindAction(
            AttackAction,
            ETriggerEvent::Started,
            this,
            &ABasePlayerController::Attack
        );
    }
}

void ABasePlayerController::Move(const FInputActionValue& Value)
{
    ABaseCharacter* Character =
        Cast<ABaseCharacter>(GetPawn());

    if (!IsValid(Character))
        return;

    const FVector2D Input = Value.Get<FVector2D>();

    Character->MoveCharacter(Input);
}

void ABasePlayerController::Look(
    const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();

    AddYawInput(Input.X);
    AddPitchInput(Input.Y);
}

void ABasePlayerController::Dash()
{
    ABaseCharacter* Character =
        Cast<ABaseCharacter>(GetPawn());

    if (!IsValid(Character))
        return;

    Character->Dash();
}

void ABasePlayerController::StartJump()
{
    if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetPawn()))
    {
        Character->Jump();
    }
}

void ABasePlayerController::StopJump()
{
    if (ABaseCharacter* Character = Cast<ABaseCharacter>(GetPawn()))
    {
        Character->StopJumping();
    }
}

void ABasePlayerController::Attack()
{
    ABaseCharacter* Character =
        Cast<ABaseCharacter>(GetPawn());

    if (!IsValid(Character))
        return;

    Character->Attack();
}