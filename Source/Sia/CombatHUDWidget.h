#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatHUDWidget.generated.h"

class UCanvasPanel;
class UProgressBar;
class UTextBlock;

USTRUCT()
struct FCombatDamageNumber
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UTextBlock> Label;
    FVector WorldPosition = FVector::ZeroVector;
    float Age = 0.f;
    float HorizontalOffset = 0.f;
};

UCLASS()
class SIA_API UCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetHealth(float Current, float Maximum);
    void ShowDamage(float Damage, const FVector& WorldPosition);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UCanvasPanel> DamageCanvas;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> HealthProgress;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> HealthValueText;
    UPROPERTY() TArray<FCombatDamageNumber> DamageNumbers;
};
