-- 모든 Stage Goal이 공유한다. Stage는 월드 Y축에 1,000 단위로 배치한다.
local STAGE_SPACING = 1000
local LAST_STAGE = 5
local state = script_state

function BeginPlay()
    state.used = false
end

function OnBeginOverlap(other)
    if state.used or other.Name ~= "IcePlayer" then
        return
    end

    local stageNumber = math.floor(obj.Location.y / STAGE_SPACING) + 1
    if stageNumber >= LAST_STAGE then
        state.used = true
        print("[Stage] 모든 Stage 완료")
        return
    end

    local nextSpawn = FVector(0, stageNumber * STAGE_SPACING, 0.5)
    state.used = true
    _G.RespawnLocation = nextSpawn

    other:ResetMovement()
    other.Velocity = FVector(0, 0, 0)
    other.Location = nextSpawn
    _G.bIsMoving=false
    
    local events = _G.GameEvents
    if events and events.ResetCamera then
        events.ResetCamera()
    end
end
