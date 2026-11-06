-- SlidingMovementComponent가 있는 플레이어에 연결한다. W/S: X축, A/D: Y축.
local RESPAWN_DELAY = 1.0
local CLEAR_FADE_SECONDS <const> = 0.5
local GROUND_MOVE_SPEED <const> = 3.0
local ICE_MOVE_SPEED <const> = 6.0
local player = script_state -- 이 플레이어의 사망 상태와 시작 위치. 핫 리로드에도 유지한다.
local targetLocation = FVector(0, 0, 0)
local targetDirection = FVector(0, 0, 0) -- 입력 규칙을 적용한 월드 이동 방향. 빙판 전환으로 다시 반전하지 않는다.
_G._G.bIsMoving = false
local currentIceType = nil
local OnRightMousePressed

local restartWidget = nil
local statsWidget = nil
local deathCountText = nil
local timerText = nil

local deathCount = 0
local playedTime = 0

local bCleared = false

-- 카메라 전환
local playerCamera = nil

local bodyComponent = nil

local function InitMovementState()
    targetLocation = FVector(0, 0, 0)
    targetDirection = FVector(0, 0, 0)
    currentIceType = nil
    obj:SetOnIce(false)
    obj:SetMoveSpeed(GROUND_MOVE_SPEED)
    _G._G.bIsMoving = false

    local events = _G.GameEvents
    if events and events.ResetCamera then
        events.ResetCamera()
    end
end

local function ResetCameraLag()
    local events = _G.GameEvents
    if events and events.ResetPlayerCameraLag then
        events.ResetPlayerCameraLag()
    end
end

local function Respawn()
    if bCleared then return end
    obj:ResetMovement()
    obj.Location = _G.RespawnLocation or player.spawnLocation
    player.isDead = false
    player.respawnTime = 0

    -- 부활 시 뒤집힌 상태를 원상복귀
    if player.aliveRotation then
        bodyComponent.Rotation = player.aliveRotation
    end
    if player.aliveLocation then
        bodyComponent.Location = player.aliveLocation
    end

    -- 화면도 원상태로 복귀
    if playerCamera then
        SetViewTargetWithBlend(
            playerCamera, 0.1, "EaseInOut", 2.0)
        print("camera transition success")
    else
        print("camera transition failed")
    end

    obj:SetMovementEnabled(true)
    InitMovementState()

    -- 리스타트 위젯 숨기기
    if restartWidget and restartWidget:IsValid() then
        restartWidget.Visible = false
    end

    bCleared = false
end

local function CompleteGame()
    if bCleared then return end
    bCleared = true
    _G.bIsGameStarted = false
    _G.bIsMoving = false
    _G.ClearResult = { elapsedSeconds = playedTime, deathCount = deathCount }

    obj:SetMovementEnabled(false)
    obj:ResetMovement()
    obj.Velocity = FVector(0, 0, 0)
    targetDirection = FVector(0, 0, 0)
    targetLocation = obj.Location

    if restartWidget and restartWidget:IsValid() then
        restartWidget.Visible = false
    end
    if statsWidget and statsWidget:IsValid() then
        statsWidget.Visible = false
    end

    StartCoroutine(function()
        local fade = GetActorByName("FadeInOutSequenceActor")
        if fade and fade:IsValid() then fade:PlaySequence() end
        wait(CLEAR_FADE_SECONDS)
        if not obj:IsValid() then return end
        -- 다음 프레임에 레벨을 바꾸는 동안 기존 페이드가 다시 밝아지지 않게 한다.
        if fade and fade:IsValid() then
            fade:SeekSequence(CLEAR_FADE_SECONDS)
            fade:PauseSequence()
        end
        OpenLevel("IceClear")
    end)
end

local function ShowRestartWidget()
    if bCleared then return end
    if restartWidget and restartWidget:IsValid() then
        restartWidget:Find("TextLabel"):SetText("실패!")
        restartWidget.Visible = true
    end
end

-- 개발자 테스트용: 가장 가까운 StageN_Start를 현재 Stage로 보고 앞/뒤 Stage 시작 위치로 옮긴다.
local function MoveStage(offset)
    if bCleared then return end
    local current, nearest = nil, math.huge
    local index = 1
    local start = GetActorByName("Stage1_Start")
    while start do
        local distance = (start.Location - obj.Location):Length()
        if distance < nearest then current, nearest = index, distance end
        index = index + 1
        start = GetActorByName("Stage" .. index .. "_Start")
    end
    local target = current and GetActorByName("Stage" .. (current + offset) .. "_Start")
    if not target then
        if current and offset > 0 then CompleteGame() end
        return
    end

    _G.RespawnLocation = target.Location + FVector(0, 0, 0)
    Respawn()
    ResetCameraLag()
    print("[Dev] Stage " .. current .. " -> " .. (current + offset))
end

local function Die(wall)
    player.respawnTime = RESPAWN_DELAY

    -- 사망 시 뒤집기
    player.aliveRotation = bodyComponent.Rotation
    player.aliveLocation = bodyComponent.Location
    local rotation = bodyComponent.Rotation
    rotation.roll = rotation.roll + 180
    bodyComponent.Rotation = rotation
    -- 뒤집어도 보이도록 높이를 0.5 올린다
    local location = bodyComponent.Location
    location.z = location.z + 0.5
    bodyComponent.Location = location
    obj:SetMovementEnabled(false)

    -- 기존 ParticleActor의 아틀라스를 바꿔 사망 위치에서 한 번만 재생한다.
    local effect = GetActorByName("DeathParticle")
    if effect and effect:HasComponent("UParticleSubUVComponent")
        and effect:SetParticleTexture("Assets/SubUV/GreenBlood.png") then
        effect:SetSubUVSize(4, 4)
        effect:SetParticleFrameRate(60)
        effect.Location = obj.Location
        effect:PlayParticle(3.0) -- 기본 크기의 3배
    end

    PlaySound2D("Assets/Sounds/Die.wav")

    -- 목숨 감소, 사망 연출, 게임 오버 같은 규칙도 여기서 처리한다.
    local cause = wall and wall:IsValid() and wall.Name or "알 수 없는 충돌"
    print("[Ice] 벽 충돌로 사망: " .. cause)

    -- 스탯 업데이트
    deathCount = deathCount + 1
    if deathCountText and deathCountText:IsValid() then
        deathCountText:SetText(tostring(deathCount))
    end
end

local function DieCoroutine()
    if bCleared or player.isDead then return end
    player.isDead = true

    StartCameraShake(0.5)

    StartHitStop(1.0, 0.1) -- 사망 직전 잠깐 느려지게함
    -- 카메라 회전

    SetViewTargetWithBlend(
        obj, 1.0, "EaseInOut", 2.0)

    wait(0.1) -- 1초 후에 실제로 사망 판정

    if bCleared then return end
    Die(wall) -- 벽 충돌을 사망으로 해석하는 규칙은 Lua에 둔다.
    wait(0.3) -- 사망하고 잠시 후에 리스타트 위젯을 띄운다
    ShowRestartWidget()
end

local function UpdateIceState()
    -- World가 FCollision::Overlap으로 관리한 결과를 사용한다.
    -- 한 액터의 여러 도형 또는 여러 빙판이 겹쳐도 하나라도 남으면 빙판이다.
    local nextIceType = obj:GetOverlappingIceType() -- nil, "default", "slippery", "reverse"

    -- 이동 컴포넌트에는 기존처럼 얼음 여부를 전달한다
    obj:SetOnIce(nextIceType ~= nil)

    -- 종류가 같으면 진입 처리를 반복하지 않는다
    if nextIceType == currentIceType then
        return
    end

    local previousIceType = currentIceType
    currentIceType = nextIceType

    -- 빙판 종류는 다음 클릭의 해석만 바꾼다. 이미 결정한 진행 방향은
    -- 진입/이탈 경계에서도 유지해야 클릭 없는 유턴과 경계 왕복이 생기지 않는다.
    if not currentIceType then
        -- 이탈 즉시 이전 클릭과 조향을 비워 다음 클릭에 reverse 상태가 남지 않게 한다.
        obj:ResetMovement()
        targetDirection = FVector(0, 0, 0)
        targetLocation = obj.Location
        _G.bIsMoving = false
    end

    print(
        "[Ice] "
        .. tostring(previousIceType)
        .. " -> "
        .. tostring(currentIceType)
    )

    obj:SetMoveSpeed(ICE_MOVE_SPEED)
    if currentIceType == "default" then
        print("[Ice] Entered default ice")
    elseif currentIceType == "slippery" then
        print("[Ice] Entered slippery ice")
    elseif currentIceType == "reverse" then
        print("[Ice] Entered reverse ice")
    elseif currentIceType == nil then
        print("[Ice] Exited ice")
        obj:SetMoveSpeed(GROUND_MOVE_SPEED)
    else
        print("[Ice] Entered unknown ice type: " .. tostring(currentIceType))
    end
end

local function GoToTitle()
    if bCleared then return end
    PlaySound2D("Assets/Sounds/Click.wav")
    OpenLevel("TitleScene")
end

local function CinematicCoroutine()
    local cinematic = GetActorByName("StartLevelCinematicActor")
    cinematic:PlaySequence()

    -- 시퀀스 시간 만큼 대기
    wait(6)
    _G.bIsGameStarted = true
end

function BeginPlay()
    _G.bIsGameStarted = false
    _G.ClearResult = nil
    bCleared = false
    deathCount = 0
    playedTime = 0
    -- StartCoroutine(CinematicCoroutine)

    playerCamera = GetActorByName("PlayerCamera")
    bodyComponent = GetComponentByName("PlayerMesh")

    _G.RespawnLocation = nil
    player.spawnLocation = obj.Location
    player.isDead = false
    player.respawnTime = 0

    restartWidget = CreateWidget("Assets/UI/RestartWidget.widget")
    restartWidget.Visible = false
    restartWidget:AddToViewport()
    restartWidget:Find("RestartButton"):BindClick(function()
        if bCleared then return end
        PlaySound2D("Assets/Sounds/Click.wav")
        Respawn()
    end)
    restartWidget:Find("TitleButton"):BindClick(GoToTitle)

    statsWidget = CreateWidget("Assets/UI/StatsWidget.widget")
    statsWidget:AddToViewport()
    statsWidget.Visible = false
    deathCountText = statsWidget:Find("DeathCount")
    deathCountText:SetText("0")
    timerText = statsWidget:Find("Timer")
    timerText:SetText("0")

    obj:SetMovementEnabled(true)
    print("[Ice] WASD: 카메라이동 / R: 시작 위치로 복귀")

    InitMovementState()
    UpdateIceState()

SetCameraRelativeTransform(obj,
    FVector(-4.0, 0.0, 2.0),
    FRotator(15.0, 0.0, 0.0)
)
    -- delegate 등록
    _G.GameEvents = _G.GameEvents or {}
    _G.GameEvents.RespawnPlayer = Respawn
    _G.GameEvents.CompleteGame = CompleteGame
end

function EndPlay()
    if restartWidget and restartWidget:IsValid() then
        restartWidget:Destroy()
    end
    if statsWidget and statsWidget:IsValid() then
        statsWidget:Destroy()
    end
    if _G.GameEvents then
        if _G.GameEvents.RespawnPlayer then
            _G.GameEvents.RespawnPlayer = nil
        end
        if _G.GameEvents.CompleteGame == CompleteGame then
            _G.GameEvents.CompleteGame = nil
        end
    end
end

function OnReload()
    if not player.spawnLocation then player.spawnLocation = obj.Location end
    Respawn()
    UpdateIceState()
end

function Tick(dt)
    if bCleared or not _G.bIsGameStarted then
        return
    end
    statsWidget.Visible = true

    -- 스탯 업데이트
    -- 플레이어가 살아있을 때만 시간이 흐르게 한다
    if not player.isDead and not bCleared then
        playedTime = playedTime + dt
        if timerText and timerText:IsValid() then
            timerText:SetText(string.format("%.2f", playedTime))
        end
    end
if IsKeyPressed("1") then
    local ok = SetViewTargetWithBlend(obj, 1.0, "EaseInOut", 2.0)
    print("[CameraTest] 맹독충 시점 요청:", ok)
end

-- 2: 기존 플레이어 카메라 시점으로 복귀
if IsKeyPressed("2") then
    if playerCamera then
        local ok = SetViewTargetWithBlend(
            playerCamera, 1.5, "EaseInOut", 2.0)
        print("[CameraTest] 기존 시점 복귀 요청:", ok)
    else
        print("[CameraTest] PlayerCamera를 찾지 못했습니다.")
    end
end
    if IsKeyPressed("r") then
        Respawn()
        return
    end
    if IsKeyPressed("q") then MoveStage(-1) return end
    if IsKeyPressed("e") then MoveStage(1) return end

    -- 자동 리스폰 비활성화
    -- if player.isDead then
    --     player.respawnTime = player.respawnTime - dt
    --     if player.respawnTime <= 0 then Respawn() end
    --     return
    -- end

    -- 모든 Begin/EndOverlap 처리가 끝난 목록을 읽는다. 인접 빙판 사이의
    -- 일시적인 빈 목록을 이탈로 처리하지 않고, 새 클릭보다 먼저 상태를 갱신한다.
    UpdateIceState()

    -- 이동 후/Overlap 갱신 전인 코루틴에서 클릭을 처리하면 이전 빙판 규칙이
    -- 적용될 수 있다. 이동 전 Tick에서 상태 확인 -> 클릭 해석 -> 이동을 처리한다.
    if IsRightMouseButtonPressed() then
        OnRightMousePressed()
    end

    -- 얼음위가 아니면 매 프레임 도착지점까지 이동 입력을 준다
    if not obj.IsOnIce and _G.bIsMoving then
        local direction = (targetLocation - obj.Location)
        if direction:Length() > 0.1 then
            -- 이 방향으로 빙판에 진입하면 그대로 미끄러진다(reverse 포함).
            targetDirection = direction:Normalize()
            obj:SetMoveInput(targetDirection)
        else
            -- 남은 거리가 0.1 미만이면 정지
            obj:SetMoveInput(FVector(0, 0, 0))
            targetDirection = FVector(0, 0, 0)
            _G.bIsMoving = false
            print("Actor reached to the target location")
        end
    elseif _G.bIsMoving then
        -- 얼음 위면 처음 지정한 방향 그대로 간다

        obj:SetMoveInput(targetDirection:Normalize())
    else -- bIsMoving == false
        obj:SetMoveInput(FVector(0, 0, 0))
    end

    -- 실제 이동 방향으로 회전한다.
    local steeringDirection = obj:GetSteeringDirection()
    -- 방향이 0이면 회전하지 않는다
    if steeringDirection:Length() > 0.01 then
        local rotation = obj.Rotation
        if currentIceType == "reverse" then
            rotation:LookDirection(-steeringDirection)
        else
            rotation:LookDirection(steeringDirection)
        end
        obj.Rotation = rotation
    end
end

function OnHit(wall)
    if bCleared then return end
    StartCoroutine(DieCoroutine)
end

OnRightMousePressed = function()
    if bCleared or player.isDead then return end
    if currentIceType == "slippery" then
        print("Mouse input is disabled on the slippery ice")
        return
    end


    local x, y = GetMousePosition()
    print("[OnRightMousePressed] " .. x, y)

    local origin, direction = ScreenToWorldRay(x, y)
    if not origin or not direction then return end
    print("start: ", origin)
    print("direction: ", direction)

    local hitPosition = Raycast(origin, direction)
    if hitPosition then
        print("hit world position: ",
            hitPosition.x,
            hitPosition.y,
            hitPosition.z)

        print("start move")
            
        targetLocation = FVector(hitPosition.x, hitPosition.y, obj.Location.z)
        -- 반전은 새 클릭을 이동 명령으로 변환하는 이 지점에서만 한 번 적용한다.
        local clickDirection = targetLocation - obj.Location
        targetDirection = currentIceType == "reverse" and -clickDirection or clickDirection
        _G.bIsMoving = true;
        print("target location: ", targetLocation)
        print("target direction: ", targetDirection)
        --move_to(targetLocation, 10)
    end
end
