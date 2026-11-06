-- IceClear.scene의 ClearSequenceActor에 연결한다. 카메라/동작은 IceClear.sequence가 맡는다.
local RESULT_TIME <const> = 11.5 -- 마지막 0.5초는 결과창 배경과 같은 고정 구도다.
local widget = nil
local resultPanel = nil
local dialogueText = nil
local skipHint = nil
local elapsed = 0.0
local nextCue = 1
local ready = false
local finished = false
local leaving = false

local function SetLine(text)
    if dialogueText and dialogueText:IsValid() then
        dialogueText:SetText(text)
    end
end

-- 일정한 재생 속도의 시퀀스에 맞춘 효과음/자막 시간표. 종료 뒤에는 Tick이 처리하지 않는다.
local CUES <const> = {
    { 2.4, function() PlaySound2D("Assets/Sounds/Hit.wav", 0.16, 0.65) end },
    { 3.1, function() PlaySound2D("Assets/Sounds/Hit.wav", 0.19, 0.65) end },
    { 3.8, function() PlaySound2D("Assets/Sounds/Hit.wav", 0.22, 0.65) end },
    { 5.3, function() PlaySound2D("Assets/Sounds/dog.mp3", 0.65) end },
    { 6.55, function() StopAllCameraShakes(false) end },
    { 6.8, function()
        PlaySound2D("Assets/Sounds/Hit.wav", 0.5, 0.9)
        StartCameraShake(0.3)
    end },
    { 7.3, function() SetLine("…살았다!") end },
    { 8.8, function() SetLine("") end },
}

local function FinishOnce()
    if finished or leaving then return end
    finished = true
    SetLine("")
    StopAllCameraShakes(true)
    if skipHint and skipHint:IsValid() then skipHint.Visible = false end
    if resultPanel and resultPanel:IsValid() then resultPanel.Visible = true end
    PlaySound2D("Assets/Sounds/Clear.mp3")
end

local function Leave(level)
    if not finished or leaving then return end
    leaving = true
    _G.ClearResult = nil
    PlaySound2D("Assets/Sounds/Click.wav")
    OpenLevel(level)
end

local function SetResultText(name, text)
    local label = widget:Find(name)
    if label and label:IsValid() then label:SetText(text) end
end

local function IsNonNegativeNumber(value)
    return type(value) == "number" and value >= 0 and value < math.huge
end

function BeginPlay()
    elapsed, nextCue = 0.0, 1
    ready, finished, leaving = false, false, false
    _G.bIsGameStarted = false

    -- Lua 전역은 씬 이동에도 남는다. 현재 결과만 복사해서 사용하고 전달용 값은 소비한다.
    local result = _G.ClearResult
    _G.ClearResult = nil
    widget = CreateWidget("Assets/UI/IceClear.widget")
    if widget then
        resultPanel = widget:Find("ResultPanel")
        dialogueText = widget:Find("DialogueText")
        skipHint = widget:Find("SkipHint")
        if resultPanel then resultPanel.Visible = false end
        SetLine("")
        local seconds = type(result) == "table" and result.elapsedSeconds
        local deaths = type(result) == "table" and result.deathCount
        SetResultText("TimeValue", IsNonNegativeNumber(seconds) and string.format("%.2f초", seconds) or "—")
        SetResultText("DeathValue", IsNonNegativeNumber(deaths) and string.format("%d회", math.floor(deaths)) or "—")
        local restartButton = widget:Find("RestartButton")
        local titleButton = widget:Find("TitleButton")
        if restartButton then restartButton:BindClick(function() Leave("IceEscape") end) end
        if titleButton then titleButton:BindClick(function() Leave("TitleScene") end) end
        widget:AddToViewport(10)
    end

    -- 액터의 Lua BeginPlay 뒤에 시퀀스 자동 재생이 시작되므로 한 프레임 후부터 완료를 검사한다.
    StartCoroutine(function()
        wait_frame()
        ready = true
    end)
end

function Tick(dt)
    if finished or leaving then return end
    elapsed = elapsed + dt
    if not ready then return end

    if IsKeyPressed("space") or IsKeyPressed("enter") then
        -- StopSequence는 카메라를 복원한다. 최종 프레임을 평가하고 Pause로 그 구도를 유지한다.
        if obj:SeekSequence(RESULT_TIME) then
            obj:PauseSequence()
            FinishOnce()
        else
            print("[Clear] 결과 구도로 이동하지 못했습니다.")
        end
        return
    end

    if not obj:IsSequencePlaying() then
        FinishOnce()
        return
    end

    while nextCue <= #CUES and elapsed >= CUES[nextCue][1] do
        CUES[nextCue][2]()
        nextCue = nextCue + 1
    end
end

function EndPlay()
    leaving = true
    finished = true
    if widget and widget:IsValid() then widget:Destroy() end
    widget, resultPanel, dialogueText, skipHint = nil, nil, nil, nil
end
