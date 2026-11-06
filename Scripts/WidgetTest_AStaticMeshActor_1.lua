-- 액터마다 별도 environment에서 실행된다. obj는 이 스크립트가 붙은 액터다.
local Seq = GetActorByName("ALevelSequenceActor_0")

function BeginPlay()
    mywidget = CreateWidget("Assets/UI/MinsangUI.widget")
    mywidget:AddToViewport();
    mywidget:Find("MinsangButton"):BindClick(function() Seq:PlaySequence() PlaySound2D("Assets/Sounds/Click.wav") end)
end

function EndPlay()
    if hud and hud:IsValid() then hud:Destroy() end
end

function OnBeginOverlap(OtherActor)
    print("[BeginOverlap] " .. OtherActor.UUID)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
end
