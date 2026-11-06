-- 액터마다 별도 environment에서 실행된다. obj는 이 스크립트가 붙은 액터다.

function BeginPlay()
    print("시작 위치", obj.Location)              -- FVector가 문자열로 찍힘
    StartCoroutine(function()
        while obj:IsValid() do
            move_to(obj.Location + FVector(5, 0, 0), 2.0)
            wait_until_move_done()
            wait(1.0)
            move_to(obj.Location - FVector(5, 0, 0), 2.0)
            wait_until_move_done()
            wait(1.0)
        end
    end)
    print("[BeginPlay] " .. obj.UUID)
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function OnBeginOverlap(OtherActor)
    print("[BeginOverlap] " .. OtherActor.UUID)
end

function OnEndOverlap(OtherActor)
end

function Tick(dt)
    if IsKeyDown("a") then print("key a is Down!") end
    if IsKeyPressed("s") then print("key s is Pressed!") end
    if IsKeyReleased("d") then print("key d is Released!") end
end
