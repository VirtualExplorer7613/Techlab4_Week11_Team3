-- 이 스크립트를 거리 판정의 중심이 될 Actor에 부착한다.
-- IcePlayer의 현재 위치를 매 Tick 읽고, 범위 안에 있는 동안 Shake를 반복 재생한다.
-- 범위를 벗어나면 추가 재생을 멈추고, 이미 시작한 Shake는 자연스럽게 종료한다.

local PLAYER_NAME = "IcePlayer"
local TRIGGER_DISTANCE = 15.0 -- 이 Actor와 플레이어 사이의 월드 거리
local SHAKE_SCALE = 0.2     -- StartCameraShake에 전달할 강도 배율
local SHAKE_INTERVAL = 0.5  -- 현재 바인딩의 1초 Shake를 조금 겹쳐 이어서 재생한다.

local player = nil
local shakeCooldown = 0.0

local function FindPlayer()
    player = GetActorByName(PLAYER_NAME)
    return player ~= nil and player:IsValid()
end

function BeginPlay()
    shakeCooldown = 0.0

    if FindPlayer() then
        print("[ProximityShake] Tracking " .. PLAYER_NAME)
    else
        print("[ProximityShake] " .. PLAYER_NAME .. " not found; retrying in Tick.")
    end
end

function Tick(dt)
    -- 플레이어가 나중에 생성되거나 파괴 후 다시 생성되어도 재검색한다.
    if player == nil or not player:IsValid() then
        shakeCooldown = 0.0
        if not FindPlayer() then
            return
        end
    end

    -- 위치를 복사해 보관하지 않고 매 프레임 읽어 이동 중인 플레이어를 추적한다.
    local offset = player.Location - obj.Location
    local distanceSquared = offset.x * offset.x
        + offset.y * offset.y
        + offset.z * offset.z
    local isInsideRange = distanceSquared <= TRIGGER_DISTANCE * TRIGGER_DISTANCE

    if not isInsideRange then
        shakeCooldown = 0.0
        return
    end

    -- 매 프레임 생성하지 않고 일정 간격으로 재생해 중첩 개수를 제한한다.
    -- 진입할 때는 즉시 실행하고, 범위 안에 머무르면 계속 반복한다.
    shakeCooldown = math.max(0.0, shakeCooldown - dt)
    if shakeCooldown <= 0.0 then
        if not StartCameraShake(SHAKE_SCALE) then
            print("[ProximityShake] StartCameraShake failed.")
        end
        shakeCooldown = SHAKE_INTERVAL
    end
end

function EndPlay()
    player = nil
    shakeCooldown = 0.0
end
