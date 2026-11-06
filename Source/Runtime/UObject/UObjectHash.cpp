#include "EnginePCH.h"
#include "UObjectHash.h"

#include "UObject/Object.h"
#include "UObject/Class.h"

//클래스에 해당하는 UObject들의 리스트(O(1)에 바로 가져옴)
static TMap<const UClass*, TSet<UObject*>>& GetClassToObjects() { static TMap<const UClass*, TSet<UObject*>> M; return M; }

//bIsIncludeDerivedClass가 켜졌을 때 자식 클래스 목록이 필요
static TMap<const UClass*, TSet<UClass*>>& GetClassToChildren() { static TMap<const UClass*, TSet<UClass*>> M; return M; }

static TMap<FString, UClass*>& GetClassMap()
{
	static TMap<FString, UClass* > Map;
	return Map;
}

void RegisterClass(UClass* Class)
{
	GetClassMap()[Class->Name] = Class;
	if (Class->Super)
	{
		GetClassToChildren()[Class->Super].Add(Class);
	}
}

UClass* FindClass(const FString& Name)
{
	UClass** Found = GetClassMap().Find(Name);
	return Found ? *Found : nullptr;
}

UObject* FindObjectWithOuter(const UObject* outer, const UClass* Class, FName Name)
{
	for (UObject* Obj : GUObjectArray)
	{
		if (Obj && Obj->GetOuter() == outer && Obj->GetFName() == Name && Obj->GetClass() == Class)
			return Obj;
	}
	return nullptr;
}

void GetObjectsOfClass(const UClass* ClassToLookFor, TArray<UObject*>& Results, bool bIsIncludeDerivedClass)
{
	if (!ClassToLookFor) return;

	Results.Reserve(Results.Num() + GUObjectArray.Num());
	//Class에 해당하는 Objects 리스트를 가져온다.
	if (const TSet<UObject*>* Objects = GetClassToObjects().Find(ClassToLookFor))
	{
		for (UObject* Obj : *Objects)
		{
			Results.Add(Obj);
		}
	}

	if (!bIsIncludeDerivedClass) return;

	//자식 클래스인 목록을 가져온다
	if (const TSet<UClass*>* ChildClasses = GetClassToChildren().Find(ClassToLookFor))
	{
		for (UClass* Child : *ChildClasses)
		{
			GetObjectsOfClass(Child, Results, bIsIncludeDerivedClass);
		}
	}
}

void HashObject(UObject* Obj, const UClass* Class)
{
	GetClassToObjects()[Class].Add(Obj);
}

void UnhashObject(UObject* Obj, const UClass* Class)
{
	if (TSet<UObject*>* Objects = GetClassToObjects().Find(Class))
		Objects->Remove(Obj);
}

void GetDerivedClasses(const UClass* ClassToLookFor, TArray<UClass*>& Results, bool bRecursive)
{
	if(const TSet<UClass*>* Children = GetClassToChildren().Find(ClassToLookFor))
		for (UClass* Child : *Children)
		{
			Results.Add(Child);
			if (bRecursive) GetDerivedClasses(Child, Results, true);
		}
}
bool TryGetLiveObjectUUID(const UObject* Object, const UClass* ObjectClass, uint32& OutUUID)
{
	OutUUID = 0;

	if (!Object)
		return false;

	// 클래스 정보를 알고 있다면 빠른 검색표를 사용한다.
	if (ObjectClass)
	{
		const TSet<UObject*>* Objects =
			GetClassToObjects().Find(ObjectClass);

		if (!Objects)
			return false;

		// 기존 집합이 UObject*를 보관하므로 타입만 맞춘다.
		// 객체를 수정하는 용도로 사용하지 않는다.
		UObject* ObjectKey = const_cast<UObject*>(Object);

		// 주소가 등록되어 있는지 먼저 확인한다.
		if (!Objects->Contains(ObjectKey))
			return false;

		// 존재를 확인한 뒤에만 객체 내부를 읽는다.
		OutUUID = Object->GetUUID();
		return true;
	}

	// 클래스 정보가 없는 객체는 기존 방식으로 확인한다.
	// 예: 생성자 안에서 구독했거나 직접 new로 만든 객체
	for (const UObject* LiveObject : GUObjectArray)
	{
		if (LiveObject && LiveObject == Object)
		{
			OutUUID = LiveObject->GetUUID();
			return true;
		}
	}

	return false;
}