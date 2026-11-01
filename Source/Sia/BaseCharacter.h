
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

    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    UFUNCTION(BlueprintPure, Category = "Combat")
    float GetHealth() const { return Health; }
    UFUNCTION(BlueprintPure, Category = "Combat")
    float GetMaxHealth() const { return MaxHealth; }


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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.f;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Combat")
    float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
    float AttackDamage = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Physics", meta = (ClampMin = "0.0"))
    float AttackImpulse = 60000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Physics", meta = (ClampMin = "0.0", Units = "cm/s"))
    float AttackKnockbackSpeed = 300.f;

    // Combat
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UBoxComponent> AttackHitbox;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float AttackCooldown = 0.8f;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    bool bIsAttack = false;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float WalkSpeed = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float JumpZVelocity = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash",
        meta = (ClampMin = "0.0", Units = "cm"))
    float DashDistance = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash",
        meta = (ClampMin = "0.01", Units = "s"))
    float DashDuration = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
    float DashCooldown = 0.8f;

private:
    UFUNCTION()
    void OnAttackHitboxOverlap(
        UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    void ResetAttack();
    void ResetDash();

    TSet<TWeakObjectPtr<AActor>> HitActorsThisAttack;

    FVector2D MoveInput = FVector2D::ZeroVector;

    bool bCanAttack = true;
    bool bCanDash = true;

    FTimerHandle AttackCooldownHandle;
    FTimerHandle DashCooldownHandle;
};
