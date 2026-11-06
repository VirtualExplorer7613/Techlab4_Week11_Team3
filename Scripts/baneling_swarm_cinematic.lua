-- BanelingSwarm.scene의 Cinematic(ALevelSequenceActor)에 연결한다. obj는 시퀀스 액터 자신.
-- 시퀀스(bAutoPlay)가 끝나면 IceEscape로 넘어간다. Space/Enter/Esc로 건너뛸 수 있다.
-- 시퀀스는 검은 화면(Fade 1)으로 끝나고, IceEscape의 IceIntro.sequence가 검은 화면에서 페이드 인한다.
local NEXT_LEVEL <const> = "IceEscape"

-- 에그 부화 순간 셰이크: { 시퀀스 시간(초), 세기 }. BanelingSwarm.sequence의 부화 키와 맞춘다
local HATCH_SHAKES <const> = {
    { 3.0, 0.7 },   -- Egg_Hero (클로즈업)
    { 3.9, 0.2 }, { 4.15, 0.2 }, { 4.4, 0.2 }, { 4.65, 0.2 }, { 4.9, 0.2 }, { 5.1, 0.2 },
}

local bLeaving = false

local function GoNext()
    if bLeaving then return end
    bLeaving = true
    OpenLevel(NEXT_LEVEL)
end

local function WaitForEnd()
    -- BeginPlay의 자동 재생이 시작된 뒤부터 검사한다
    wait_frame()
    wait_until(function() return not obj:IsSequencePlaying() end)
    GoNext()
end

local function HatchShake()
    local now = 0.0
    for _, shake in ipairs(HATCH_SHAKES) do
        wait(shake[1] - now)
        StartCameraShake(shake[2])
        now = shake[1]
    end
end

function BeginPlay()
    bLeaving = false
    StartCoroutine(WaitForEnd)
    StartCoroutine(HatchShake)
end

function Tick(dt)
    if IsKeyPressed("space") or IsKeyPressed("enter") or IsKeyPressed("escape") then
        GoNext()
    end
end
