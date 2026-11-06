#include "EnginePCH.h"
#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"

UCameraModifier_CameraShake::UCameraModifier_CameraShake() = default;

UCameraModifier_CameraShake::~UCameraModifier_CameraShake()
{
	RemoveAllCameraShakes(true);
}

// Super로 Alpha를 갱신하고 각 Shake를 BasePOV 사본에 적용한 뒤 그 차이만 최종 POV에 더합니다.
// 앞선 Shake가 바꾼 회전을 다음 Shake의 기준으로 쓰면 재생 순서에 따라 방향이 달라집니다.
bool UCameraModifier_CameraShake::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);
    if (IsDisabled())
    {
		RemoveAllCameraShakes(true);
        return false;
    }
	const FMinimalViewInfo BasePOV = InOutPOV;
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		FMinimalViewInfo ShakePOV = BasePOV;
		Shake->UpdateAndApplyCameraShake(DeltaTime, GetAlpha(), ShakePOV);
		InOutPOV.Location += ShakePOV.Location - BasePOV.Location;
		InOutPOV.Rotation.Pitch += ShakePOV.Rotation.Pitch - BasePOV.Rotation.Pitch;
		InOutPOV.Rotation.Yaw += ShakePOV.Rotation.Yaw - BasePOV.Rotation.Yaw;
		InOutPOV.Rotation.Roll += ShakePOV.Rotation.Roll - BasePOV.Rotation.Roll;
		InOutPOV.FOV += ShakePOV.FOV - BasePOV.FOV;
	}
	RemoveFinishedShakes();
	if (ActiveShakes.IsEmpty()) Super::DisableModifier(true);
	return false;
}

// 클래스,상속,생성 가능 여부를 검사하고 NewObject, CDO 설정 복사, StartShake를 거쳐 목록에 넣습니다.
UCameraShakeBase* UCameraModifier_CameraShake::AddCameraShake(UClass* ShakeClass, float Scale)
{
	if (!ShakeClass || !ShakeClass->IsChildOf(UCameraShakeBase::StaticClass()) || !ShakeClass->Constructor)
		return nullptr;
	UCameraShakeBase* Shake = NewObject<UCameraShakeBase>(this, ShakeClass);
	if (!Shake) return nullptr;

	// NewObject는 이 엔진에서 CDO 설정을 자동 복사하지 않는다.
	const auto* Defaults = static_cast<const UCameraShakeBase*>(ShakeClass->GetDefaultObject());

	if (Defaults){ Shake->CopySettingsFrom(*Defaults); }

	Shake->StartShake(Scale);
	if (Shake->IsFinished()) { delete Shake; return nullptr; }
	ActiveShakes.Add(Shake);	
	EnableModifier();
	return Shake;
}

void UCameraModifier_CameraShake::DisableModifier(bool bImmediate)
{
	Super::DisableModifier(bImmediate);
	if (IsDisabled()) RemoveAllCameraShakes(bImmediate);
}

// 이 목록에 속한 인스턴스인지 확인하고 StopShake를 요청한 뒤 종료 항목을 정리합니다.
void UCameraModifier_CameraShake::RemoveCameraShake(UCameraShakeBase* ShakeInstance, bool bImmediately)
{
	if (ActiveShakes.Find(ShakeInstance) < 0) return;
	ShakeInstance->StopShake(bImmediately);
	RemoveFinishedShakes(); // 순회 중에는 삭제하지 않고, 안전한 시점에 종료 항목을 제거합니다.
}

// 살아 있는 모든 인스턴스에 StopShake 적용. 안전한 시점에 종료 항목 해제.
void UCameraModifier_CameraShake::RemoveAllCameraShakes(bool bImmediately)
{
	for (UCameraShakeBase* Shake : ActiveShakes)
	{
		uint32 UUID = 0;
		if (TryGetLiveObjectUUID(Shake, nullptr, UUID)) // 이미 삭제된 포인터는 건너뜁니다.
			Shake->StopShake(bImmediately);
	}
	RemoveFinishedShakes();
}

// 역순으로 목록을 순회해 완료된 항목을 먼저 목록에서 빼고 delete합니다.
// 전역 종료 시 이미 삭제된 포인터도 건너뜁니다.
void UCameraModifier_CameraShake::RemoveFinishedShakes()
{
	for (int32 i = ActiveShakes.Num() - 1; i >= 0; --i)
	{
		UCameraShakeBase* Shake = ActiveShakes[i];
		uint32 UUID = 0;
		const bool bIsLive = TryGetLiveObjectUUID(Shake, nullptr, UUID);
		if (!bIsLive || Shake->IsFinished())
		{
			ActiveShakes.RemoveAt(i);
			delete Shake;
		}	
	}
}
