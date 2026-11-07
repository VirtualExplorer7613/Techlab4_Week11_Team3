#include "EnginePCH.h"
#include "AnimSequenceBase.h"

#include "Animation/AnimData/AnimDataModel.h"
#include "UObject/UObjectGlobals.h"

bool UAnimSequenceBase::InitializeModel()
{
    // 이미 만들어졌으면 기존 모델을 유지한다.
    if (DataModel)
    {
        return true;
    }

    DataModel = NewObject<UAnimDataModel>(this);

    return DataModel != nullptr;
}