#include "EnginePCH.h"
#include "LuaBindings.h"

#include <sol/sol.hpp>
#include <cmath>

#include "Components/MovementComponent.h"
#include "Components/SlidingMovementComponent.h"
#include "Components/IceVolumeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ParticleSubUVComponent.h"
#include "Core/EngineTimer.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/InputSystem.h"
#include "Script/LuaActorRef.h"
#include "Script/LuaWidgetRef.h"
#include "Script/CoroutineScheduler.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "UI/UserWidget.h"
#include "UI/Button.h"
#include "UI/TextBlock.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Asset/AssetManager.h"
#include "Components/AudioComponent.h"
#include "Engine/Classes/Sound/SoundWave.h"
#include "Camera/CameraShakeBase.h"

#include "LevelSequence/LevelSequenceActor.h" 
#include "LevelSequence/LevelSequencePlayer.h"

namespace
{
	struct FLuaClickBinding { AActor* Owner; FLuaWidgetRef Button; };
	TArray<FLuaClickBinding> ClickBindings;

	TArray<FLuaActorRef> PendingDestroies;

	AActor* OwnerFromEnv(sol::this_environment& ThisEnv)
	{
		if (!ThisEnv)
			return nullptr;

		sol::environment& Env = ThisEnv;
		sol::optional<FLuaActorRef> Ref = Env["obj"];
		return Ref
			? Ref->Get()
			: nullptr;
	}


	UWorld* WorldFromEnv(sol::this_environment& ThisEnv, const char* FuncName)
	{
		AActor* Owner = OwnerFromEnv(ThisEnv);
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		if (!World)
			throw sol::error(FString(FuncName) + " must be called from an actor script.");
		return World;
	}

	static ULevelSequencePlayer* SequencePlayerOf(const FLuaActorRef& Ref)
	{
		ALevelSequenceActor* SeqActor = Cast<ALevelSequenceActor>(&Deref(Ref));
		if (!SeqActor) throw sol::error("Actor is not a LevelSequenceActor.");
		if (!SeqActor->GetSequencePlayer()) SeqActor->InitializePlayer();
		return SeqActor->GetSequencePlayer();
	}

	void RegisterAudio(sol::state& Lua)
	{
		Lua.set_function("PlaySound2D", [](sol::this_environment ThisEnv, const FString& Path,
			sol::optional<float> Volume, sol::optional<float> Pitch)
			{
				if (USoundWave* Sound = UAssetManager::Get().LoadSound(Path))
					UGameplayStatics::PlaySound2D(WorldFromEnv(ThisEnv, "PlaySound2D"), Sound,
						Volume.value_or(1.0f), Pitch.value_or(1.0f));
			});

		Lua.set_function("PlaySoundAtLocation", [](sol::this_environment ThisEnv, const FString& Path,
			const FVector& Location, sol::optional<float> Volume, sol::optional<float> Pitch)
			{
				if (USoundWave* Sound = UAssetManager::Get().LoadSound(Path))
					UGameplayStatics::PlaySoundAtLocation(WorldFromEnv(ThisEnv, "PlaySoundAtLocation"), Sound,
						Location, Volume.value_or(1.0f), Pitch.value_or(1.0f));
			});
	}

	void RegisterGameplay(sol::state& Lua)
	{
		// 실제 전환은 다음 Tick 시작. 호출한 뒤의 코드도 이번 프레임에는 그대로 실행된다.
		Lua.set_function("OpenLevel", [](sol::this_environment ThisEnv, const FString& LevelName) {
			UGameplayStatics::OpenLevel(WorldFromEnv(ThisEnv, "OpenLevel"), LevelName);
			});
		Lua.set_function("SetGameValue", [](sol::this_environment ThisEnv, const FString& Key, double Value) {
			if (UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldFromEnv(ThisEnv, "SetGameValue")))
				GameInstance->SetValue(Key, Value);
			});
		Lua.set_function("GetGameValue", [](sol::this_environment ThisEnv, const FString& Key, sol::optional<double> Default) {
			const double Fallback = Default.value_or(0.0);
			UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldFromEnv(ThisEnv, "GetGameValue"));
			return GameInstance ? GameInstance->GetValue(Key, Fallback) : Fallback;
			});
	}

	void RegisterWidget(sol::state& Lua)
	{
		Lua.new_usertype<FLuaWidgetRef>("Widget", sol::no_constructor,
			"IsValid", [](const FLuaWidgetRef& R) { return R.Get() != nullptr; },
			"Name", sol::property([](const FLuaWidgetRef& R) { return DerefWidget<UWidget>(R).WidgetName; }),
			"Visible", sol::property(
				[](const FLuaWidgetRef& R) { return DerefWidget<UWidget>(R).IsVisible(); },
				[](const FLuaWidgetRef& R, bool b) { DerefWidget<UWidget>(R).Visibility = b ? 0 : 1; }),
			"Find", [](const FLuaWidgetRef& R, const std::string& Name) -> sol::optional<FLuaWidgetRef>
			{
				UWidget* Found = DerefWidget<UUserWidget>(R).FindWidget(Name);
				return Found ? sol::optional(FLuaWidgetRef::Make(Found)) : sol::nullopt;
			},
			"AddToViewport", [](const FLuaWidgetRef& R, sol::optional<int> Z) { DerefWidget<UUserWidget>(R).AddToViewport(Z.value_or(0)); },
			"RemoveFromViewport", [](const FLuaWidgetRef& R) { DerefWidget<UUserWidget>(R).RemoveFromViewport(); },
			"Destroy", [](const FLuaWidgetRef& R) { DerefWidget<UUserWidget>(R).Destroy(); },
			"SetText", [](const FLuaWidgetRef& R, const std::string& T) { DerefWidget<UTextBlock>(R).SetText(T); },
			"BindClick", [](sol::this_environment Env, const FLuaWidgetRef& R, sol::protected_function Fn)
			{
				AActor* Owner = OwnerFromEnv(Env);
				if (!Owner) throw sol::error("BindClick needs an Actor script context.");
				UButton& Button = DerefWidget<UButton>(R);
				const FLuaActorRef OwnerRef = FLuaActorRef::Make(Owner);
				Button.OnClicked = [OwnerRef, Fn]
					{
						if (!OwnerRef.Get()) return;              // 액터가 이미 지워졌다
						sol::protected_function_result Result = Fn();
						if (!Result.valid())
						{
							sol::error Err = Result;
							HTR_LOG(Error, "[Lua] OnClicked: {}", Err.what());
						}
					};
				ClickBindings.Add({ Owner, R });
			});

		Lua.set_function("CreateWidget", [](sol::this_environment Env, const std::string& Path) -> sol::optional<FLuaWidgetRef>
			{
				AActor* Owner = OwnerFromEnv(Env);
				UUserWidget* W = Owner ? ::CreateWidget(Owner->GetWorld(), Path) : nullptr;
				return W ? sol::optional(FLuaWidgetRef::Make(W)) : sol::nullopt;
			});
	}

	// FVector, FRotator
	void RegisterMath(sol::state& Lua)
	{
		Lua.new_usertype<FVector>("FVector",
			sol::call_constructor,
			sol::constructors<FVector(), FVector(float, float, float)>(),
			"x", &FVector::X,
			"y", &FVector::Y,
			"z", &FVector::Z,
			sol::meta_function::addition, [](const FVector& A, const FVector& B) { return A + B; },
			sol::meta_function::subtraction, [](const FVector& A, const FVector& B) { return A - B; },
			sol::meta_function::unary_minus, [](const FVector& A) { return -A; },

			sol::meta_function::multiplication, sol::overload(
				[](const FVector& V, float Scalar) { return V * Scalar; },
				[](const float Scalar, const FVector& V) { return V * Scalar; }
			),
			sol::meta_function::division, [](const FVector& V, float Scalar) { return V / Scalar; },
			sol::meta_function::equal_to, [](const FVector& A, const FVector& B) { return A == B; },
			sol::meta_function::to_string, [](const FVector& V)
			{ return std::format("({:.2f}, {:.2f}, {:.2f})", V.X, V.Y, V.Z); },

			"Length", &FVector::Length,
			"Normalize", [](const FVector& V) { return V.GetSafeNormal(); },
			"Dot", [](const FVector& A, const FVector& B) { return A.Dot(B); },
			"Cross", [](const FVector& A, const FVector& B) { return A.Cross(B); }
		);

		Lua.new_usertype<FRotator>("FRotator",
			sol::call_constructor,
			sol::constructors<FRotator(), FRotator(float, float, float)>(),
			"pitch", &FRotator::Pitch,
			"yaw", &FRotator::Yaw,
			"roll", &FRotator::Roll,

			"LookDirection", &FRotator::LookDirection,
			sol::meta_function::to_string, [](const FRotator& R)
			{ return std::format("(P={:.1f}, Y={:.1f}, R={:.1f})", R.Pitch, R.Yaw, R.Roll); }
		);
	}

	UMovementComponent* FindFirstMovement(AActor& Actor)
	{
		for (UActorComponent* Comp : Actor.GetComponents())
		{
			if (UMovementComponent* MoveComp = Cast<UMovementComponent>(Comp))
			{
				return MoveComp;
			}
		}
		return nullptr;
	}

	USlidingMovementComponent& SlidingMovement(const FLuaActorRef& Ref)
	{
		for (UActorComponent* Comp : Deref(Ref).GetComponents())
			if (auto* Movement = Cast<USlidingMovementComponent>(Comp)) return *Movement;
		throw sol::error("Actor does not have a SlidingMovementComponent.");
	}

	UParticleSubUVComponent& ParticleComponent(const FLuaActorRef& Ref)
	{
		for (UActorComponent* Comp : Deref(Ref).GetComponents())
			if (auto* Particle = Cast<UParticleSubUVComponent>(Comp)) return *Particle;
		throw sol::error("Actor does not have a ParticleSubUVComponent.");
	}

	USpringArmComponent& CameraArm(const FLuaActorRef& Ref)
	{
		for (UActorComponent* Comp : Deref(Ref).GetComponents())
			if (auto* Arm = Cast<USpringArmComponent>(Comp)) return *Arm;
		throw sol::error("Actor does not have a SpringArmComponent.");
	}

	void RegisterActor(sol::state& Lua)
	{
		Lua.new_usertype<FLuaActorRef>("Actor",
			sol::no_constructor, // 액터는 Lua에서 생성할 수 없음

			"PlaySequence", [](const FLuaActorRef& Ref) { if (ULevelSequencePlayer* P = SequencePlayerOf(Ref)) P->Play(); },
			"PauseSequence", [](const FLuaActorRef& Ref) { if (ULevelSequencePlayer* P = SequencePlayerOf(Ref)) P->Pause(); },
			"StopSequence", [](const FLuaActorRef& Ref) { if (ULevelSequencePlayer* P = SequencePlayerOf(Ref)) P->Stop(); },
			"SeekSequence", [](const FLuaActorRef& Ref, float Seconds) -> bool
			{
				ALevelSequenceActor* Actor = Cast<ALevelSequenceActor>(Ref.Get());
				if (!Actor || !std::isfinite(Seconds)) return false;
				if (!Actor->GetSequencePlayer() && !Actor->InitializePlayer()) return false;
				ULevelSequencePlayer* Player = Actor->GetSequencePlayer();
				// Clamp before converting seconds to an integer frame number.
				Seconds = FMath::Clamp(Seconds, static_cast<float>(Player->GetStartTime().AsSeconds()),
					static_cast<float>(Player->GetEndTime().AsSeconds()));
				// Jump evaluates the destination pose/camera without firing skipped events.
				Player->SetPlaybackPosition(
					FMovieSceneSequencePlaybackParams(Seconds, EUpdatePositionMethod::Jump));
				return true;
			},
			"IsSequencePlaying", [](const FLuaActorRef& Ref)
			{
				ULevelSequencePlayer* P = SequencePlayerOf(Ref);
				return P && P->IsPlaying();
			},
			"PlayAudio", [](const FLuaActorRef& Ref)
			{
				for (UActorComponent* Component : Deref(Ref).GetComponents())
					if (UAudioComponent* Audio = Cast<UAudioComponent>(Component)) { Audio->Play(); return; }
			},
			"StopAudio", [](const FLuaActorRef& Ref)
			{
				for (UActorComponent* Component : Deref(Ref).GetComponents())
					if (UAudioComponent* Audio = Cast<UAudioComponent>(Component)) { Audio->Stop(); return; }
			},
			"UUID", sol::property([](const FLuaActorRef& Ref) { return Ref.UUID; }),
			"IsValid", [](const FLuaActorRef& Ref) { return Ref.Get() != nullptr; },
			"Name", sol::property([](const FLuaActorRef& Ref) { return Deref(Ref).GetName(); }),
			"HasComponent", [](const FLuaActorRef& Ref, const FString& ClassName)
			{
				for (UActorComponent* Component : Deref(Ref).GetComponents())
				{
					if (!Component || !Component->IsRegistered()) continue;
					for (UClass* Class = Component->GetClass(); Class; Class = Class->Super)
						if (Class->Name == ClassName) return true;
				}
				return false;
			},
			"GetComponentRelativeScale", [](const FLuaActorRef& Ref, const FString& Name) -> sol::optional<FVector>
			{
				for (UActorComponent* Component : Deref(Ref).GetComponents())
					if (Component && Component->GetName() == Name)
						if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
							return SceneComponent->GetRelativeScale3D();
				return sol::nullopt;
			},
			"SetComponentRelativeScale", [](const FLuaActorRef& Ref, const FString& Name, const FVector& Scale)
			{
				for (UActorComponent* Component : Deref(Ref).GetComponents())
					if (Component && Component->GetName() == Name)
						if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
						{
							SceneComponent->SetRelativeScale3D(Scale);
							return true;
						}
				return false;
			},
			"GetOverlappingActors", [](const FLuaActorRef& Ref, sol::this_state State)
			{
				TArray<AActor*> Actors;
				Deref(Ref).GetOverlappingActors(Actors);
				sol::state_view Lua(State);
				sol::table Result = Lua.create_table();
				for (int32 Index = 0; Index < Actors.Num(); ++Index)
					Result[Index + 1] = FLuaActorRef::Make(Actors[Index]);
				return Result;
			},
			"GetIceType", [](const FLuaActorRef& Ref) -> sol::optional<FString>
			{
				for (UActorComponent* Component : Deref(Ref).GetComponents())
				{
					if (!Component || !Component->IsRegistered())
						continue;

					if (auto* Ice = Cast<UIceVolumeComponent>(Component))
					{
						return Ice->GetIceType();
					}
				}
				return sol::nullopt;
			},
			"GetOverlappingIceType", [](const FLuaActorRef& Ref) -> sol::optional<FString>
			{
				AActor& Actor = Deref(Ref);

				for (UActorComponent* Component : Actor.GetComponents())
				{
					auto* Primitive = Cast<UPrimitiveComponent>(Component);
					if (!Primitive || !Primitive->IsRegistered())
						continue;

					// OverlapInfos는 들어간 순서로 쌓인다. 두 빙판 경계에 걸쳐 있을 때 막 들어간 빙판을 따르도록 뒤에서부터 본다.
					const auto& Infos = Primitive->GetOverlapInfos();
					for (int32 Index = Infos.Num() - 1; Index >= 0; --Index)
					{
						auto* OtherComp = Infos[Index].OtherComp;
						if (!OtherComp || !OtherComp->IsRegistered())
							continue;

						AActor* OtherActor = OtherComp->GetOwner();
						if (!OtherActor ||
							OtherActor == &Actor ||
							OtherActor->GetWorld() != Actor.GetWorld())
						{
							continue;
						}

						if (auto* Ice = Cast<UIceVolumeComponent>(OtherComp))
						{
							return Ice->GetIceType();
						}
					}
				}
				return sol::nullopt;
			},
			"IsOnIce", sol::property([](const FLuaActorRef& Ref) { return SlidingMovement(Ref).IsOnIce(); }),
			"SetOnIce", [](const FLuaActorRef& Ref, bool bOnIce) { SlidingMovement(Ref).SetOnIce(bOnIce); },
			"IsMovementEnabled", sol::property([](const FLuaActorRef& Ref) { return SlidingMovement(Ref).IsMovementEnabled(); }),
			"SetMovementEnabled", [](const FLuaActorRef& Ref, bool bEnabled) { SlidingMovement(Ref).SetMovementEnabled(bEnabled); },
			"ResetMovement", [](const FLuaActorRef& Ref) { SlidingMovement(Ref).ResetMovement(); },
			"SetMoveInput", [](const FLuaActorRef& Ref, const FVector& Direction) { SlidingMovement(Ref).SetMoveInput(Direction); },
			"GetSteeringDirection", [](const FLuaActorRef& Ref) { return SlidingMovement(Ref).GetSteeringDirection(); },
			"SetMoveSpeed", [](const FLuaActorRef& Ref, float MoveSpeed)
			{
				SlidingMovement(Ref).MoveSpeed = MoveSpeed;
			},
			"GetMoveSpeed", [](const FLuaActorRef& Ref)
			{
				return SlidingMovement(Ref).MoveSpeed;
			},
			"SetParticleTexture", [](const FLuaActorRef& Ref, const FString& Path) { return ParticleComponent(Ref).SetParticleTexture(Path); },
			"SetSubUVSize", [](const FLuaActorRef& Ref, int32 Cols, int32 Rows)
			{
				if (Cols < 1 || Rows < 1 || Cols > 4096 || Rows > 4096)
					throw sol::error("SubUV columns and rows must be between 1 and 4096.");
				ParticleComponent(Ref).SetSubUVSize(Cols, Rows);
			},
			"SetParticleFrameRate", [](const FLuaActorRef& Ref, float FPS) { ParticleComponent(Ref).SetFrameRate(FPS); },
			"PlayParticle", [](const FLuaActorRef& Ref, sol::optional<float> Scale) { ParticleComponent(Ref).PlayOnce(Scale.value_or(1.0f)); },

			"Location", sol::property(
				// Getter
				[](const FLuaActorRef& Ref) { return Deref(Ref).GetActorLocation(); },
				// Setter
				[](const FLuaActorRef& Ref, const FVector& V)
				{
					if (USceneComponent* RootComp = Deref(Ref).GetRootComponent())
					{
						RootComp->SetWorldLocation(V, false);
					}
				}),
			"Rotation", sol::property(
				// Getter
				[](const FLuaActorRef& Ref) { return Deref(Ref).GetActorRotation(); },
				// Setter
				[](const FLuaActorRef& Ref, const FRotator& R)
				{
					if (USceneComponent* RootComp = Deref(Ref).GetRootComponent())
					{
						RootComp->SetWorldRotation(R, false);
					}
				}),
			// Velocity는 MovementComponent가 있는 경우에만 제공
			"Velocity", sol::property(
				// Getter
				[](const FLuaActorRef& Ref)
				{
					if (UMovementComponent* MoveComp = FindFirstMovement(Deref(Ref)))
					{
						return MoveComp->Velocity;
					}
					return FVector::ZeroVector;
				},
				// Setter
				[](const FLuaActorRef& Ref, const FVector& V)
				{
					if (UMovementComponent* MoveComp = FindFirstMovement(Deref(Ref)))
					{
						MoveComp->Velocity = V;
					}
					else
					{
						throw sol::error("Actor does not have a MovementComponent to set Velocity.");
					}
				}),
			"PrintLocation", [](const FLuaActorRef& Ref)
			{
				const FVector Location = Deref(Ref).GetActorLocation();
				HTR_LOG(Info, "[Lua] {} ({:.2f}, {:.2f}, {:.2f})", Deref(Ref).GetName(), Location.X, Location.Y, Location.Z);
			},

			"Destroy", [](const FLuaActorRef& Ref)
			{
				// 이미 파괴되지 않은 액터만 PendingDestroies에 추가
				if (Ref.Get())
				{
					PendingDestroies.Add(Ref);
				}
			},

			sol::meta_function::equal_to, [](const FLuaActorRef& A, const FLuaActorRef& B)
			{
				return A.UUID == B.UUID;
			},
			sol::meta_function::to_string, [](const FLuaActorRef& Ref)
			{
				return std::format("Actor#{}", Ref.UUID);
			},

			"CameraLagEnabled", sol::property(
				[](const FLuaActorRef& Ref)
				{
					return CameraArm(Ref).bEnableCameraLag;
				},
				[](const FLuaActorRef& Ref, bool b)
				{
					CameraArm(Ref).bEnableCameraLag = b;
				}
			),
			"ResetCameraLag", [](const FLuaActorRef& Ref)
			{
				CameraArm(Ref).ResetCameraLag();
			}
		);

		Lua.set_function("GetActorByName", [](sol::this_environment Env, const std::string& Name) -> sol::optional<FLuaActorRef>
			{
				AActor* Owner = OwnerFromEnv(Env);
				if (!Owner) throw sol::error("GetActorByName needs an Actor script context.");
				UWorld* World = Owner->GetWorld();
				if (!World) throw sol::error("GetActorByName needs an Actor script context.");
				for (AActor* Actor : World->GetCurrentLevel()->GetActors())
				{
					if (Actor && Actor->GetName() == Name)
					{
						return FLuaActorRef::Make(Actor);
					}
				}
				return sol::nullopt;
			});
		Lua.set_function("GetComponentByName", [](sol::this_environment Env, const std::string& Name) -> sol::optional<FLuaComponentRef>
			{
				AActor* Owner = OwnerFromEnv(Env);
				if (!Owner) throw sol::error("GetComponentByName needs an Actor script context.");
				for (UActorComponent* Component : Owner->GetComponents())
				{
					if (Component && Component->GetName() == Name)
					{
						return FLuaComponentRef::Make(Component);
					}
				}
				return sol::nullopt;
			});

		Lua.set_function("SetViewTargetWithBlend",
			[](sol::this_environment Env,
				const FLuaActorRef& Target,
				sol::optional<float> Time,
				sol::optional<std::string> Function,
				sol::optional<float> Exp) -> bool
			{
				AActor* Owner = OwnerFromEnv(Env);
				UWorld* World = Owner ? Owner->GetWorld() : nullptr;
				APlayerController* PC = World ? World->GetPlayerController() : nullptr;
				APlayerCameraManager* PCM = PC ? PC->GetCameraManager() : nullptr;

				AActor* TargetActor = Target.Get();
				if (!PCM || !TargetActor || TargetActor->GetWorld() != World)
					return false;

				FViewTargetTransitionParams Params;
				Params.BlendTime = Time.value_or(0.0f);
				Params.BlendExp = Exp.value_or(2.0f);

				const std::string Mode = Function.value_or("Cubic");

				if (Mode == "Linear")
					Params.BlendFunction = EViewTargetBlendFunction::Linear;
				else if (Mode == "EaseIn")
					Params.BlendFunction = EViewTargetBlendFunction::EaseIn;
				else if (Mode == "EaseOut")
					Params.BlendFunction = EViewTargetBlendFunction::EaseOut;
				else if (Mode == "EaseInOut")
					Params.BlendFunction = EViewTargetBlendFunction::EaseInOut;
				else if (Mode != "Cubic")
					return false;

				PCM->SetViewTarget(TargetActor, Params);
				return true;
			});

		Lua.set_function("IsCameraBlending",
			[](sol::this_environment Env) -> bool
			{
				AActor* Owner = OwnerFromEnv(Env);
				UWorld* World = Owner ? Owner->GetWorld() : nullptr;
				APlayerController* PC = World ? World->GetPlayerController() : nullptr;
				APlayerCameraManager* PCM = PC ? PC->GetCameraManager() : nullptr;

				if (!PCM)
					throw sol::error("IsCameraBlending: camera manager is unavailable.");

				return PCM->IsBlending();
			});
		Lua.set_function("SetCameraRelativeTransform",
			[](const FLuaActorRef& Ref,
				const FVector& RelativeLocation,
				const FRotator& RelativeRotation)
			{
				AActor& Actor = Deref(Ref);

				for (UActorComponent* Component : Actor.GetComponents())
				{
					if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
					{
						Camera->SetRelativeLocationAndRotation(
							RelativeLocation,
							RelativeRotation
						);
						return;
					}
				}

				throw sol::error(
					"SetCameraRelativeTransform: Actor has no camera component."
				);
			});
	}

	void RegisterComponent(sol::state& Lua)
	{
		Lua.new_usertype<FLuaComponentRef>("Component",
			sol::no_constructor,
			"IsValid", [](const FLuaComponentRef& R) { return R.Get() != nullptr; },
			"Name", sol::property([](const FLuaComponentRef& R) { return Deref(R).GetName(); }),
			"Owner", sol::property([](const FLuaComponentRef& R) { return FLuaActorRef::Make(Deref(R).GetOwner()); }),
			"Location", sol::property(
				// Getter
				[](const FLuaComponentRef& Ref)
				{
					if (auto* SceneComp = Cast<USceneComponent>(&Deref(Ref)))
					{
						return SceneComp->GetRelativeLocation();
					}
					else
					{
						throw sol::error("Component is not a scene component.");
					}
				},
				// Setter
				[](const FLuaComponentRef& Ref, const FVector& V)
				{
					if (auto* SceneComp = Cast<USceneComponent>(&Deref(Ref)))
					{
						SceneComp->SetRelativeLocation(V);
					}
					else
					{
						throw sol::error("Component is not a scene component.");
					}
				}),
			"Rotation", sol::property(
				// Getter
				[](const FLuaComponentRef& Ref) {
					if (auto* SceneComp = Cast<USceneComponent>(&Deref(Ref)))
					{
						return SceneComp->GetRelativeRotation();
					}
					else
					{
						throw sol::error("Component is not a scene component.");
					}
				},
				// Setter
				[](const FLuaComponentRef& Ref, const FRotator& R)
				{
					if (auto* SceneComp = Cast<USceneComponent>(&Deref(Ref)))
					{
						SceneComp->SetRelativeRotation(R);
					}
					else
					{
						throw sol::error("Component is not a scene component.");
					}
				})
		);
	}

	EKeyCode ToKeyCode(const std::string& Name)
	{
		if (Name.size() == 1 && std::isalnum(static_cast<unsigned char>(Name[0])))
		{
			// 알파벳 또는 숫자 키를 EKeyCode로 변환
			return static_cast<EKeyCode>(std::toupper(static_cast<unsigned char>(Name[0])));
		}

		static const TMap<std::string, EKeyCode> KeyMap =
		{
			{ "space", EKeyCode::Space },
			{ "enter", EKeyCode::Enter },
			{ "escape", EKeyCode::Escape },
			{ "tab", EKeyCode::Tab },
			{ "shift", EKeyCode::Shift },
			{ "backspace", EKeyCode::Backspace },
			{ "ctrl", EKeyCode::Control },
			{ "alt", EKeyCode::Alt },
			{ "left", EKeyCode::Left },
			{ "right", EKeyCode::Right },
			{ "up", EKeyCode::Up },
			{ "down", EKeyCode::Down },
		};

		std::string Lower = Name;
		std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char c) { return std::tolower(c); });
		if (const EKeyCode* Code = KeyMap.Find(Lower))
		{
			return *Code;
		}
		throw sol::error("Unknown key name: " + Name);
	}

	void RegisterInput(sol::state& Lua)
	{
		Lua.set_function("IsKeyDown", [](const std::string& K) {
			return FInputSystem::IsKeyDown(ToKeyCode(K));
			});
		Lua.set_function("IsKeyPressed", [](const std::string& K) {
			return FInputSystem::IsKeyPressed(ToKeyCode(K));
			});
		Lua.set_function("IsKeyReleased", [](const std::string& K) {
			return FInputSystem::IsKeyReleased(ToKeyCode(K));
			});

		Lua.set_function("IsLeftMouseButtonPressed", []() {
			return FInputSystem::IsMousePressed(EMouseButton::Left);
			});

		Lua.set_function("IsRightMouseButtonPressed", []() {
			return FInputSystem::IsMousePressed(EMouseButton::Right);
			});

		// Viewport 포지션을 반환한다.
		Lua.set_function("GetMousePosition", [](sol::this_environment ThisEnv) {
			AActor* Owner = OwnerFromEnv(ThisEnv);
			if (!Owner)
			{
				throw sol::error("GetMousePosition must be called from a coroutine with an Actor context.");
			}

			UWorld* World = Owner->GetWorld();
			if (!World)
			{
				throw sol::error("GetMousePosition must be called from a coroutine with an Actor context.");
			}

			FWorldContext* Context =
				GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;

			if (!Context || !Context->GameViewport)
			{
				throw sol::error("GetMousePosition must be called from a coroutine with an Actor context.");
			}

			FGameViewportClient* Viewport = Context->GameViewport.get();

			const FRect& ViewportRect = Viewport->GetRect();

			//const float MouseX = FInputSystem::GetMouseX();
			//const float MouseY = FInputSystem::GetMouseY();
			POINT Mouse{};
			if (!::GetCursorPos(&Mouse))
			{
				throw sol::error("Failed to get mouse cursor position.");
			}

			const float LocalX = static_cast<float>(Mouse.x) - ViewportRect.X;
			const float LocalY = static_cast<float>(Mouse.y) - ViewportRect.Y;

			return std::make_tuple(
				LocalX,
				LocalY);
			});
		Lua.set_function("GetViewportSize",
			[](sol::this_environment ThisEnv) -> std::tuple<float, float>
			{
				AActor* Owner = OwnerFromEnv(ThisEnv);
				if (!Owner)
				{
					throw sol::error("GetMousePosition must be called from a coroutine with an Actor context.");
				}

				UWorld* World = Owner->GetWorld();
				if (!World)
				{
					throw sol::error("GetMousePosition must be called from a coroutine with an Actor context.");
				}

				FWorldContext* Context =
					GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;

				if (!Context || !Context->GameViewport)
				{
					throw sol::error("GetMousePosition must be called from a coroutine with an Actor context.");
				}

				FGameViewportClient* Viewport = Context->GameViewport.get();

				const FRect& ViewportRect = Viewport->GetRect();

				return  std::make_tuple(
					static_cast<float>(ViewportRect.Width),
					static_cast<float>(ViewportRect.Height));
			}
		);

		Lua.set_function("ScreenToWorldRay",
			[](sol::this_environment Env, float X, float Y)
			-> std::tuple<sol::optional<FVector>, sol::optional<FVector>>
			{
				AActor* Owner = OwnerFromEnv(Env);
				UWorld* World = Owner ? Owner->GetWorld() : nullptr;

				if (!GEngine || !World)
				{
					return {};
				}

				FWorldContext* Context =
					GEngine->GetWorldContextFromWorld(World);

				if (!Context || !Context->GameViewport)
				{
					return {};
				}

				FRay Ray;
				if (!Context->GameViewport->ScreenToWorldRay(X, Y, &Ray))
				{
					return {};
				}

				return {
					sol::optional<FVector>(Ray.Origin),
					sol::optional<FVector>(Ray.Direction)
				};
			}
		);

		Lua.set_function("Raycast",
			[](sol::this_environment Env,
				const FVector& Origin,
				const FVector& Direction) -> sol::optional<FVector>
			{
				AActor* Owner = OwnerFromEnv(Env);
				UWorld* World = Owner ? Owner->GetWorld() : nullptr;

				if (!World || Direction.Dot(Direction) <= 0.0000001f)
				{
					return sol::nullopt;
				}

				FRay Ray{
					Origin,
					Direction.Normalized()
				};

				FHitResult Hit;
				if (!World->LineTraceSingle(Ray, Hit))
				{
					return sol::nullopt;
				}

				return Hit.ImpactPoint;
			});

		// Tick(dt)의 dt를 쓰는 게 원칙. 코루틴처럼 dt를 못 받는 곳에서만 쓴다
		Lua.set_function("GetDeltaTime", []() {
			return EngineTimer::GetDeltaTime();
			});
		Lua.set_function("GetTime", []() {
			return EngineTimer::GetTotalTime();
			});
	}

	void RegisterCoroutine(sol::state& Lua)
	{
		Lua.set_function("StartCoroutine",
			[](sol::this_environment ThisEnv, sol::function Fn)
			{
				FCoroutineScheduler::Get().Start(OwnerFromEnv(ThisEnv), Fn);
			});

		// C++ 함수가 반환값을 yield 값으로 남기고 멈춘다. 코루틴 밖에서 부르면 Lua 에러
		Lua.set_function("wait", sol::yielding(
			[](float Seconds)
			{
				return std::make_tuple("time", Seconds);
			}));
		Lua.set_function("wait_frame", sol::yielding(
			[]()
			{
				return;
			}));
		Lua.set_function("wait_until", sol::yielding(
			[](sol::function Cond)
			{
				return std::make_tuple("until", Cond);
			}));
		Lua.set_function("wait_until_move_done", sol::yielding(
			[]()
			{
				return "move";
			}));

		Lua.set_function("move_to",
			[](sol::this_environment ThisEnv, const FVector& Target, float Speed)
			{
				if (AActor* Owner = OwnerFromEnv(ThisEnv))
				{
					FCoroutineScheduler::Get().MoveTo(Owner, Target, Speed);
				}
			});
	}

	void RegisterCameraAction(sol::state& Lua)
	{
		Lua.set_function("StartHitStop",
			[](sol::this_environment Env, float Duration, float Dilation)
			{
				WorldFromEnv(Env, "StartHitStop")->StartHitStop(Duration, Dilation);
			});

		Lua.set_function("StartCameraShake",
			[](sol::this_environment Env, float Scale) -> bool
			{
				UWorld* World = WorldFromEnv(Env, "StartCameraShake");
				APlayerController* PC = World->GetPlayerController();
				APlayerCameraManager* PCM = PC ? PC->GetCameraManager() : nullptr;

				if (!PCM) return false;

				UClass* ShakeClass = UCameraShakeBase::StaticClass();
				auto* Settings = static_cast<UCameraShakeBase*>(
					ShakeClass->GetDefaultObject());

				if (!Settings) return false;

				// Lua에서는 Scale로 세기를 조절한다.
				Settings->Duration = 1.0f;
				Settings->BlendInTime = 0.05f;
				Settings->BlendOutTime = 0.3f;
				Settings->RotationPitch.Amplitude = 2.0f;
				Settings->RotationPitch.Frequency = 10.0f;
				Settings->RotationYaw.Amplitude = 1.0f;
				Settings->RotationYaw.Frequency = 13.0f;

				return PCM->StartCameraShake(ShakeClass, Scale) != nullptr;
			});

		Lua.set_function("StopAllCameraShakes",
			[](sol::this_environment Env, bool bImmediately)
			{
				UWorld* World = WorldFromEnv(Env, "StopAllCameraShakes");
				APlayerController* PC = World->GetPlayerController();
				APlayerCameraManager* PCM = PC ? PC->GetCameraManager() : nullptr;

				if (PCM)
				{
					PCM->StopAllCameraShakes(bImmediately);
				}
			});

		Lua.set_function("PlayFOVPunch",
			[](sol::this_environment Env,
				float Strength,
				float InTime,
				float HoldTime,
				float OutTime) -> bool
			{
				UWorld* World = WorldFromEnv(Env, "PlayFOVPunch");
				APlayerController* PC = World->GetPlayerController();
				APlayerCameraManager* PCM =
					PC ? PC->GetCameraManager() : nullptr;

				if (!PCM) return false;

				return PCM->PlayFOVPunch(
					Strength, InTime, HoldTime, OutTime) != nullptr;
			});

		Lua.set_function("StopFOVPunch",
			[](sol::this_environment Env)
			{
				UWorld* World = WorldFromEnv(Env, "StopFOVPunch");
				APlayerController* PC = World->GetPlayerController();
				APlayerCameraManager* PCM =
					PC ? PC->GetCameraManager() : nullptr;

				if (PCM)
					PCM->StopFOVPunch(true);
			});
	}
}

namespace FLuaBindings
{
	void Register(sol::state& Lua)
	{
		// TODO(B): wait, wait_until, wait_until_move_done, StartCoroutine
		RegisterMath(Lua);
		RegisterActor(Lua);
		RegisterComponent(Lua);
		RegisterInput(Lua);
		RegisterCoroutine(Lua);
		RegisterWidget(Lua);
		RegisterGameplay(Lua);
		RegisterAudio(Lua);
		RegisterCameraAction(Lua);
	}

	void FlushPendingDestroy(UWorld* World)
	{
		TArray<FLuaActorRef> ToDestroy = std::move(PendingDestroies);
		PendingDestroies = TArray<FLuaActorRef>();

		for (const FLuaActorRef& Ref : ToDestroy)
		{
			if (Ref.World != World)
			{
				continue;
			}
			if (AActor* Actor = Ref.Get()) // 살아 있는 액터만 Destroy
			{
				Actor->Destroy();
			}
		}
	}

	void ReleaseCallbacks(AActor* Owner)
	{
		for (int32 i = ClickBindings.Num() - 1; i >= 0; --i)
		{
			if (ClickBindings[i].Owner != Owner) continue;
			if (UButton* B = Cast<UButton>(ClickBindings[i].Button.Get()))
				B->OnClicked = nullptr;
			ClickBindings.RemoveAt(i);
		}
	}
}
