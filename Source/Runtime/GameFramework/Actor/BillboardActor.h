#pragma once

#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"
#include "BillboardActor.generated.h"

UCLASS()
class ABillboardActor : public AActor
{
	GENERATED_BODY()
public:
	ABillboardActor();
	virtual ~ABillboardActor();

	inline UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; };
private:
	UBillboardComponent* BillboardComponent;
};
