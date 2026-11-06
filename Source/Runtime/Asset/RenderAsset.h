#pragma once

#include "UObject/Object.h"
#include "UObject/Class.h"

#include "RenderAsset.generated.h"

UCLASS()
class URenderAsset : public UObject
{
	GENERATED_BODY()
public:
	URenderAsset() = default;
	virtual ~URenderAsset() = default;

	void SetPath(FString Path) { AssetPath = Path; }
	const FString& GetPath() const { return AssetPath; }

private:
	FString AssetPath;
};
