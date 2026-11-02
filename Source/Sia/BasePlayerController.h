
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InventoryWidget.h"
#include "BasePlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UCombatHUDWidget;
struct FInputActionValue;

UCLASS()
class SIA_API ABasePlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ABasePlayerController();

    void RefreshHealthUI();
    void ShowDamageNumber(float Damage, const FVector& WorldPosition);

    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void ToggleInventory();

    UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
    void SetInventoryItems(const TArray<FInventoryItemView>& Items);
    // 개발 중 콘솔에서 GiveItem Apple 12로 데이터→인벤토리→WBP 연결을 확인한다.
    UFUNCTION(Exec) void GiveItem(FName ItemId, int32 Quantity = 1);

    UFUNCTION(BlueprintPure, Category = "Inventory|UI")
    UInventoryWidget* GetInventoryWidget() const
    {
        return InventoryUI;
    }
    UCombatHUDWidget* GetCombatHUDWidget() const { return CombatHUD; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    virtual void SetupInputComponent() override;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UCombatHUDWidget> HealthHUDClass;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSoftClassPtr<UInventoryWidget> InventoryWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> MouseLookMappingContext;
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> QuickSlotMappingContext;
    UPROPERTY(Transient)
    TArray<TObjectPtr<UInputAction>> QuickSlotActions;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> MouseLookAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> DashAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> AttackAction;

    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> InventoryAction;
    UPROPERTY(EditDefaultsOnly, Category = "Input")
    TObjectPtr<UInputAction> InteractAction;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCombatHUDWidget> CombatHUD;

    UPROPERTY(Transient)
    TObjectPtr<UInventoryWidget> InventoryUI;

    UPROPERTY(Transient)
    TArray<FInventoryItemView> InventoryView;

    bool bInventoryOpen = false;

    void CreateCombatHUD();

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void StartJump();
    void StopJump();
    void Dash();
    void Attack();
    void Interact();
    void UseQuickSlot1();
    void UseQuickSlot2();
    void UseQuickSlot3();
    void UseQuickSlot4();
    void UseQuickSlot(int32 Index);
};
