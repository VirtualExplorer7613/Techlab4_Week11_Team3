#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "Core/Types.h"
#include "UObject/Object.h"
#include "UObject/UObjectHash.h"

using FDelegateHandle = uint64;

inline FDelegateHandle GNextDelegateHandleId = 1;


//기본 Delegate 타입과 선언 매크로
template<typename... Args>
class TDelegate {
public:
	using HandlerType = std::function<void(Args...)>;

	TDelegate() = default;
	TDelegate(const TDelegate&) = delete; // 복사생성 금지
	TDelegate& operator=(const TDelegate&) = delete; // 복사생성 금지
	~TDelegate() { Clear(); }

	// 일반 함수나 람다 등록
	// 람다가 캡처한 객체의 생존 여부는 자동확인하지 않는다.
	FDelegateHandle Add(const HandlerType& handler)
	{
		return AddInternal(std::move(handler), nullptr, nullptr, nullptr, 0);
	}

	// UObject를 상속받는 클래스의 멤버 함수 바인딩
	template<typename T>
	FDelegateHandle AddDynamic(T* Instance, void (T::* Func)(Args...))
	{

		// 예외처리
		static_assert(std::is_base_of_v<UObject, T>, "AddDynamic requires a UObject-derived class.");
		if (!Instance || !Func) return 0;

		const UObject* Object = Instance;
		uint32 UUID = 0;


		//등록할때는 먼저 기존배열에서 생존 여부를 확인한다.
		// 확인하기 전에 Object->Getclass()로 접근하지 않는다.
		if (!TryGetLiveObjectUUID(Object, nullptr, UUID)) return 0;

		const UClass* ObjectClass = Object->GetClass();

		if (ObjectClass)
		{
			uint32 RegistedUUID = 0;

			//실제 클래스별 검색표에 등록되어 있는지 확인한다.
			if (!TryGetLiveObjectUUID(Object, ObjectClass, RegistedUUID) || RegistedUUID != UUID) { ObjectClass = nullptr; }
			
			//검색표에 없는 객체는 기존배열검색을 사용한다.
		
		}
		return AddInternal([Instance, Func](Args... args){
				(Instance->*Func)(args...);
			},
			Object,
			Object,
			ObjectClass,
			UUID
		);
	}

	//UObejct가 아닌 일반 객체의 맴버함수 등록
	template<typename T>
	FDelegateHandle AddRaw(T* Instance, void (T::* Func)(Args...))
	{
		static_assert(!std::is_base_of_v<UObject, T>, "Use AddDynamic for UObject-derived classes.");

		if (!Instance || !Func) return 0;

		return AddInternal([Instance, Func](Args... args) {
			(Instance->*Func)(args...);
			}, Instance, nullptr, nullptr, 0);
	}

	void Remove(FDelegateHandle Handle)
	{
		if (Handle == 0) return;

		for (auto& Entry : Handlers)
		{
			if (Entry->Handle == Handle) {
				Entry->bActive = false;
				break;
			}
		}
		RemoveInactive();
	}

	// 이 Delegate에서 해당 객체가 등록한 구독을 모두 해제.
	// AddDynamic과 AddRaw로 등록한 구독에 적용된다.
	template<typename T>
	void RemoveAll(const T* Instance)
	{
		if (!Instance) return;
		const void* Owner = nullptr;

		if constexpr (std::is_base_of_v<UObject, T>)
		{
			Owner = static_cast<const UObject*>(Instance);
		}
		else
		{
			Owner = Instance;
		}

		for (auto& Entry : Handlers)
		{
			if (Entry->Instance == Owner)
			{
				Entry->bActive = false;
			}
		}

		RemoveInactive();
	}

	// 현재 호출할수 있는 구독이 하나라도 있는지 확인
	bool IsBound() const
	{
		for (const auto& Entry : Handlers)
		{
			if (IsCallable(*Entry))
				return true;
		}
		return false;
	}
	void Broadcast(Args... args)
	{
		// 실행중 등록/해제가 발생할수 있으므로 복사본을 순회한다.

		auto Snapshot = Handlers;

		for (auto& Entry : Snapshot)
		{
			if (!IsCallable(*Entry))
			{
				Entry->bActive = false;
				continue;
			}

			Entry->Handler(args...);
		}

		RemoveInactive();


	}

	// 등록된 함수 전부해제
	void Clear()
	{
		for (auto& Entry : Handlers)
		{
			Entry->bActive = false;
		}
		Handlers.clear();
	}




private:
	struct FHandler
	{
		FDelegateHandle Handle = 0; //구독번호
		HandlerType Handler; // 어느 객체의 함수인지
		const void* Instance = nullptr; //RemoveAll에서 구독자를 구별할 주소

		bool bActive = true; // 활성화 상태인지

		//UObject일시에만 사용하는 정보
		const UObject* Object = nullptr; // 어느 객체의 함수인지
		const UClass* ObjectClass = nullptr;
		uint32 UUID = 0;

	};

	FDelegateHandle AddInternal(HandlerType handler, const void* Instance, const UObject* Object, const UClass* ObjectClass, uint32 UUID)
	{
		if (!handler) return 0;

		// 번호가 순환했다면 발급중단.
		// 과거에 사용한 번호를 다시 발급하지 않는다.
		if (GNextDelegateHandleId == 0) return 0;

		auto Entry = std::make_shared<FHandler>();

		Entry->Handle = GNextDelegateHandleId++;
		Entry->Handler = std::move(handler);
		Entry->Instance = Instance;
		Entry->Object = Object;
		Entry->ObjectClass = ObjectClass;
		Entry->UUID = UUID;

		FDelegateHandle Handle = Entry->Handle;
		Handlers.push_back(std::move(Entry));
		
		return Handle;
	}


	static bool IsCallable(const FHandler& Entry)
	{
		if (!Entry.bActive)
			return false;

		// 일반 함수, 람다 또는 AddRaw 구독.
		// 대상의 생존은 사용하는 쪽에서 보장해야 한다.
		if (!Entry.Object)
			return true;

		uint32 CurrentUUID = 0;

		if (!TryGetLiveObjectUUID(
			Entry.Object,
			Entry.ObjectClass,
			CurrentUUID))
		{
			return false;
		}

		// 주소가 재사용됐더라도 다른 UUID라면 호출하지 않는다.
		return CurrentUUID == Entry.UUID;
	}

	// 비활성화된 구독을 실제 배열에서 제거
	void RemoveInactive()
	{
		Handlers.erase(
			std::remove_if(
				Handlers.begin(),
				Handlers.end(),
				[](const std::shared_ptr<FHandler>& Entry)
				{
					return !Entry->bActive;
				}
			),
			Handlers.end()
		);
	}

	std::vector<std::shared_ptr<FHandler>> Handlers;
};

#define DECLARE_DELEGATE(Name, ...) using Name = TDelegate<__VA_ARGS__>
