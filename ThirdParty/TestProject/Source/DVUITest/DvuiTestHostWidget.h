#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DvuiTestHostWidget.generated.h"

/**
 * Trivial concrete UUserWidget subclass that the test harness can construct
 * at runtime. UUserWidget itself is abstract / not-placeable, so we need a
 * concrete subclass to host UDVUIWidget when verifying the UMG path.
 */
UCLASS()
class DVUITEST_API UDvuiTestHostWidget : public UUserWidget
{
	GENERATED_BODY()
};
