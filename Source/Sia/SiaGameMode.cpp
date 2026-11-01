#include "SiaGameMode.h"
#include "BaseCharacter.h"
#include "BasePlayerController.h"
#include "UObject/ConstructorHelpers.h"

ASiaGameMode::ASiaGameMode()
{
    PlayerControllerClass = ABasePlayerController::StaticClass();
	static ConstructorHelpers::FClassFinder<ABaseCharacter> PlayerCharacter(TEXT("/Game/BP_TC"));
	DefaultPawnClass = ABaseCharacter::StaticClass();
	if (PlayerCharacter.Succeeded())
	{
		DefaultPawnClass = PlayerCharacter.Class;
	}
}
