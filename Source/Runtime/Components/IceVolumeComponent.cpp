#include "EnginePCH.h"
#include "IceVolumeComponent.h"

void UIceVolumeComponent::SetIceType(const FString& InIceType)
{
	if (InIceType == "default" ||
		InIceType == "slippery" ||
		InIceType == "reverse")
	{
		IceType = InIceType;
		return;
	}

	IceType = "default";
	HTR_LOG(Warning, "Invalid IceType '{}' set. Defaulting to 'default'.", InIceType.c_str());
}