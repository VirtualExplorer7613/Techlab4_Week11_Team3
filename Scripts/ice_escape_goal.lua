-- IceEscape 전용 Stage Goal. stage_goal.lua와 같은 일을 하지만 다음 Stage 위치를 계산하지 않는다.
-- 골 액터 이름(StageN_Goal)에서 번호를 읽고 StageN+1_Start 액터 위치로 보낸다.
-- 다음 Start가 없으면 마지막 Stage다. 그래서 Stage 개수나 간격이 바뀌어도 이 스크립트는 그대로 쓴다.
local SPAWN_HEIGHT = 0.0
local GOAL_FREEZE_SECONDS = 0.5 -- 골에 닿은 뒤 다음 Stage로 넘어가기 전까지 조작을 멈추는 시간
local state = script_state

function BeginPlay()
    state.used = false
end

function OnBeginOverlap(other)
    if state.used or other.Name ~= "IcePlayer" then
        return
    end

    local stageNumber = tonumber(string.match(obj.Name, "^Stage(%d+)_Goal$"))
    if not stageNumber then
        print("[Stage] 골 액터 이름에서 Stage 번호를 읽지 못함: " .. obj.Name)
        return
    end

    local nextStart = GetActorByName("Stage" .. (stageNumber + 1) .. "_Start")
    if not nextStart then
        local events = _G.GameEvents
        if events and events.CompleteGame then
            state.used = true
            -- 완료 기록과 조작 잠금은 골에 닿은 프레임에 확정한다.
            events.CompleteGame()
            print("[Stage] 모든 Stage 완료")
        end
        return
    end

    state.used = true
    other:SetMovementEnabled(false) -- 멈춰 있는 동안 클릭/관성/장애물 충돌을 받지 않는다
    PlaySound2D("Assets/Sounds/Levelup.mp3")
    StartCoroutine(function()
        -- 페이드 인/아웃
        local seqA = GetActorByName("FadeInOutSequenceActor")
        seqA:PlaySequence()

        wait(GOAL_FREEZE_SECONDS)
        state.used = false -- Q로 되돌아와 다시 도착해도 동작하도록 푼다
        if not other:IsValid() then return end

        local nextSpawn = nextStart.Location + FVector(0, 0, SPAWN_HEIGHT)
        _G.RespawnLocation = nextSpawn

        other:ResetMovement()
        other.Velocity = FVector(0, 0, 0)
        other.Location = nextSpawn
        _G.bIsMoving = false
        other:SetMovementEnabled(true)

        local events = _G.GameEvents
        if events and events.ResetCamera then
            events.ResetCamera()
        end
        if events and events.ResetPlayerCameraLag then
            events.ResetPlayerCameraLag()
        end
        print("[Stage] " .. stageNumber .. " -> " .. (stageNumber + 1))
    end)
end
