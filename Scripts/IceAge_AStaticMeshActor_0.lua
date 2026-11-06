-- 액터마다 별도 environment에서 실행된다. obj는 이 스크립트가 붙은 액터다.

local MoveSpeed = 3.0

function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)

    StartCoroutine(MouseInputCoroutine)
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
    ProcessKeyboardInput(dt)
end

function ProcessKeyboardInput(dt)
    if IsKeyDown('w') then
        obj.Location = obj.Location + FVector(1, 0, 0) * dt * MoveSpeed
    end
    if IsKeyDown('a') then
        obj.Location = obj.Location + FVector(0, -1, 0) * dt * MoveSpeed
    end
    if IsKeyDown('s') then
        obj.Location = obj.Location + FVector(-1, 0, 0) * dt * MoveSpeed
    end
    if IsKeyDown('d') then
        obj.Location = obj.Location + FVector(0, 1, 0) * dt * MoveSpeed
    end
end

local function OnRightMousePressed()
    local x, y = GetMousePosition()
    print("[OnRightMousePressed] " .. x, y)

    local origin, direction = ScreenToWorldRay(x, y)
    if origin then
        print("start: ", origin)
        print("direction: ", direction)
    end

    local hitPosition = Raycast(origin, direction)
    if hitPosition then
        print("hit world position: ",
            hitPosition.x,
            hitPosition.y,
            hitPosition.z)

        print("start move")
            
        local targetLocation = FVector(hitPosition.x, hitPosition.y, obj.Location.z)
        print("target location: ", targetLocation)
        move_to(targetLocation, 10)
    end
end

function MouseInputCoroutine()
    while true do
        wait_until(function()
            return IsRightMouseButtonPressed()
        end)

        OnRightMousePressed()
    end
end