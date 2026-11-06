#pragma once

#include "UI/Widget.h"

struct FLuaWidgetRef
{
    UWidget* Ptr = nullptr;
    UClass* Class = nullptr;
    uint32 UUID = 0;

    static FLuaWidgetRef Make(UWidget* W)
    {
        return W ? FLuaWidgetRef{ W, W->GetClass(), W->GetUUID() } : FLuaWidgetRef{};
    }

    UWidget* Get() const
    {
        uint32 Live = 0;
        return TryGetLiveObjectUUID(Ptr, Class, Live) && Live == UUID ? Ptr : nullptr;
    }
};

template <typename T>
T& DerefWidget(const FLuaWidgetRef& Ref)
{
    T* W = Cast<T>(Ref.Get());
    if (!W) throw sol::error("Widget is destroyed or is not a " + T::StaticClass()->Name);
    return *W;
}
