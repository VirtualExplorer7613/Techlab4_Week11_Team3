#pragma once

#include "AnimationAsset.h"
#include "AnimSequenceBase.generated.h"

class UAnimDataModel;

UCLASS()
class UAnimSequenceBase : public UAnimationAsset
{
	GENERATED_BODY()
	
public:
	UAnimSequenceBase() = default;

	bool InitializeModel();

	UAnimDataModel* GetDataModel() const { return DataModel; }

private:
	UAnimDataModel* DataModel = nullptr;
};