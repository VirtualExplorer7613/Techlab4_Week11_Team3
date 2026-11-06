-- IntroSequenceActor(ALevelSequenceActor)에 연결한다. obj는 시퀀스 액터 자신.
-- IceIntro.sequence: 레터박스 + 로우 앵글 카메라 + 뒤에서 달려오는 Ultralisk/Shiba.
-- 이 스크립트는 대사 자막, Ultralisk 발걸음 셰이크, 플레이어 입력 잠금만 맡는다.
local PLAYER_NAME <const> = "IcePlayer"
local SEQUENCE_LENGTH <const> = 6.5   -- IceIntro.sequence PlaybackEnd (초)
local STEP_INTERVAL <const> = 0.55    -- Ultralisk 발걸음 간격
local ULTRA_STOP_TIME <const> = 5.6   -- Ultralisk가 멈추는 시간 (시퀀스 키와 맞춤)

-- 대사: { 시작 시간, 끝 시간, 문장 }
local LINES <const> = {
    { 1.4, 2.8, "...쿵. 쿵." },
    { 3.2, 6.2, "이 빙판에서 살아남아야 한다.." },
}

local dialogueWidget = nil
local dialogueText = nil
local player = nil

local function SetLine(text)
    if dialogueText and dialogueText:IsValid() then
        dialogueText:SetText(text)
    end
end

local function Cleanup()
    if dialogueWidget and dialogueWidget:IsValid() then
        dialogueWidget:Destroy()
    end
    dialogueWidget = nil
    dialogueText = nil
    if player and player:IsValid() then
        player:SetMovementEnabled(true)
    end
end

-- Ultralisk가 가까워질수록 발걸음 셰이크가 세진다. 멈출 때 한 번 크게 흔든다.
local function FootstepShake()
    local elapsed = 0.0
    while elapsed < ULTRA_STOP_TIME do
        local closeness = elapsed / ULTRA_STOP_TIME
        StartCameraShake(0.25 + 0.75 * closeness)
        wait(STEP_INTERVAL)
        elapsed = elapsed + STEP_INTERVAL
    end
    StartCameraShake(2.0)
end

local function Dialogue()
    local now = 0.0
    for _, line in ipairs(LINES) do
        wait(line[1] - now)
        SetLine(line[3])
        wait(line[2] - line[1])
        SetLine("")
        now = line[2]
    end
end

local function PlayIntro()
    -- BanelingSwarm이 검은 화면으로 끝나므로 첫 프레임부터 Fade 1로 시작해야 깜빡이지 않는다 → 먼저 재생
    obj:PlaySequence()

    -- IcePlayer BeginPlay의 Respawn이 이동을 다시 켜므로 한 프레임 뒤에 잠근다
    wait_frame()

    player = GetActorByName(PLAYER_NAME)
    if player and player:IsValid() then
        player:SetMovementEnabled(false)
    else
        print("[Intro] " .. PLAYER_NAME .. " not found")
    end

    dialogueWidget = CreateWidget("Assets/UI/IntroDialogue.widget")
    if dialogueWidget then
        dialogueText = dialogueWidget:Find("DialogueText")
        SetLine("")
        dialogueWidget:AddToViewport(10)
    end

    StartCoroutine(FootstepShake)
    StartCoroutine(Dialogue)

    -- 시퀀스가 끝나면 카메라는 플레이어 카메라로, 추격자는 숨긴 위치로 돌아간다 (Restore State)
    wait(0.1)
    wait_until(function() return not obj:IsSequencePlaying() end)
    _G.bIsGameStarted = true
    Cleanup()
    print("[Intro] done (" .. SEQUENCE_LENGTH .. "s)")
end

function BeginPlay()
    StartCoroutine(PlayIntro)
end

function EndPlay()
    Cleanup()
end
