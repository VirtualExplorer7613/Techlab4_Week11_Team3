#include "EnginePCH.h"
#include "LevelSequence/LevelSequencePlayer.h"

#include "LevelSequence/LevelSequence.h"
#include "LevelSequence/LevelSequenceActor.h"
#include "MovieSceneTracks/MovieScenePropertyBinding.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

void ULevelSequencePlayer::Initialize(ULevelSequence* InLevelSequence, UWorld* InWorld, const FMovieSceneSequencePlaybackSettings& InSettings)
{
	SavedViewTarget = nullptr;
	SavedViewTargetUUID = 0;
	bHasSavedViewTarget = false;

	UMovieSceneSequencePlayer::Initialize(InLevelSequence, InWorld, InSettings);
}

ULevelSequencePlayer* ULevelSequencePlayer::CreateLevelSequencePlayer(UWorld* World, const FString& LevelSequencePath,
	const FMovieSceneSequencePlaybackSettings& Settings, ALevelSequenceActor*& OutActor)
{
	OutActor = nullptr;
	if (!World) return nullptr;
	ALevelSequenceActor* Actor = World->SpawnActor<ALevelSequenceActor>();
	if (!Actor) return nullptr;
	Actor->LevelSequenceAsset = LevelSequencePath;
	Actor->ApplyPlaybackSettings(Settings);
	Actor->InitializePlayer();
	OutActor = Actor;
	return Actor->GetSequencePlayer();
}

UCameraComponent* ULevelSequencePlayer::GetActiveCameraComponent() const
{
	return MovieSceneHelpers::CameraComponentFromRuntimeObject(CurrentCameraObject);
}

void ULevelSequencePlayer::UpdateCameraCut(UObject* CameraObject, float BlendTime)
{
    UCameraComponent* Camera =
        MovieSceneHelpers::CameraComponentFromRuntimeObject(CameraObject);

    if (World && World->IsGameWorld())
    {
        APlayerController* PC = World->GetPlayerController();
        APlayerCameraManager* PCM =
            PC ? PC->GetCameraManager() : nullptr;

        if (PCM)
        {
            AActor* CameraActor = Camera ? Camera->GetOwner() : nullptr;

            if (CameraActor)
            {
                // 첫 컷에서만 원래 시점을 저장한다.
                if (!bHasSavedViewTarget)
                {
                    const FViewTarget& Previous = PCM->GetViewTarget();

                    SavedViewTarget = Previous.Target;
                    SavedViewTargetUUID = Previous.TargetUUID;
                    bHasSavedViewTarget = true;
                }

                FViewTargetTransitionParams Params;
                Params.BlendTime = BlendTime > 0.0f ? BlendTime : 0.0f;

                PCM->SetViewTarget(CameraActor, Params);
            }
            else if (bHasSavedViewTarget)
            {
                // 컷 종료 또는 카메라 바인딩 실패: 기존 시점으로 복귀.
                AActor* RestoreTarget = nullptr;

                if (PCM->IsActorAlive(
                    SavedViewTarget, SavedViewTargetUUID))
                {
                    RestoreTarget = SavedViewTarget;
                }
                else if (PC)
                {
                    RestoreTarget = PC->GetPawn();
                }

                // nullptr이면 PCM이 PlayerController로 대체한다.
                PCM->SetViewTarget(RestoreTarget);

                SavedViewTarget = nullptr;
                SavedViewTargetUUID = 0;
                bHasSavedViewTarget = false;
            }
        }

        // 기존 뷰포트 카메라 참조도 유지한다.
        // PCM이 없는 경우에는 이 경로로 카메라 컷이 적용된다.
        if (GEngine)
        {
            if (FWorldContext* Context =
                GEngine->GetWorldContextFromWorld(World);
                Context && Context->GameViewport)
            {
                Context->GameViewport->SetCameraComponent(Camera);
            }
        }
    }

    // 컷이 끝났다면, PCM 유무와 관계없이 복귀 정보를 비운다.
    if (!Camera)
    {
        SavedViewTarget = nullptr;
        SavedViewTargetUUID = 0;
        bHasSavedViewTarget = false;
    }

    OnCameraCut.Broadcast(Camera);
}
