
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "BaseCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UCharacterAnimationComponent;
class UBoxComponent;

UCLASS()
class SIA_API ABaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ABaseCharacter();

    // PlayerController에서 호출
    void MoveCharacter(const FVector2D& Input);
    void Dash();
    void Attack();

    // 공격 판정
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EnableAttackHitbox();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void DisableAttackHitbox();

protected:
    virtual void BeginPlay() override;

    // Camera
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    // Animation
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
    TObjectPtr<UCharacterAnimationComponent> AnimationComponent;

    // Combat
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UBoxComponent> AttackHitbox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float AttackCooldown = 0.8f;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float WalkSpeed = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float JumpZVelocity = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
    float DashSpeed = 1200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
    float DashCooldown = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float AttackCooldown = 0.8f;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    bool bIsAttack = false;

private:
    void ResetAttack();
    void ResetDash();

    FVector2D MoveInput = FVector2D::ZeroVector;

    bool bCanAttack = true;
    bool bCanDash = true;

    FTimerHandle AttackCooldownHandle;
    FTimerHandle DashCooldownHandle;
};
