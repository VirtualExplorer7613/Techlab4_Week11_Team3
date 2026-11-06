## Lua types

### FVector

#### 생성자
```lua
local v = FVector(x, y, z)
```

#### 프로퍼티
```lua
local v = FVector(1, 2, 3)
v.x
v.y
v.z

v.x = v.x + 1
```

#### 함수
```lua
local v1 = FVector(1, 2, 3)
local v2 = FVector(4, 5, 6)

v1 + v2		-- FVector(5, 7, 9)
v1 - v2		-- FVector(-3, -3, -3)
v1 * 2		-- FVector(2, 4, 6)
2 * v1		-- FVector(2, 4, 6)
v1 / 2		-- FVector(0.5, 1, 1.5)
v1 == v2	-- false
tostring(v1)	-- "(1.00, 2.00, 3.00)"

v1:Length()	-- 3.7416573867739
v1:Normalize()	-- FVector(0.26726124191242, 0.53452248382485, 0.80178372573727)
v1:Dot(v2)	-- 32
v1:Cross(v2)	-- FVector(-3, 6, -3)
```

### FRotator

#### 생성자
```lua
local r = FRotator(pitch, yaw, roll)
```

#### 프로퍼티
```lua
local r = FRotator(10, 20, 30)

r.pitch
r.yaw
r.roll
```

#### 함수
```lua
local r1 = FRotator(10, 20, 30)

tostring(r1)	-- "(P=10.0, Y=20.0, R=30.0)"
```

### Actor

#### 생성자
```lua
-- Lua에서 직접 생성할 수 없음
-- obj: 현재 스크립트를 소유한 Actor
local actor = obj
```

#### 프로퍼티
```lua
local actor = obj

local uuid = actor.UUID           -- 액터 식별자 (읽기 전용)
local name = actor.Name           -- 액터 이름 (읽기 전용)
local location = actor.Location   -- 월드 위치 (FVector)
local rotation = actor.Rotation   -- 월드 회전 (FRotator)
local velocity = actor.Velocity   -- 속도 (FVector)

actor.Location = FVector(100, 200, 300)
actor.Rotation = FRotator(0, 90, 0)

-- 위치의 일부 성분을 변경하려면 수정 후 다시 대입
local position = actor.Location
position.x = position.x + 10
actor.Location = position

-- Velocity 설정에는 MovementComponent가 필요함
-- actor.Velocity = FVector(100, 0, 0)
```

#### 함수
```lua
local actor = obj

actor:IsValid()		-- 액터가 유효하면 true, 아니면 false
actor:PrintLocation()	-- 액터 이름과 월드 위치를 로그에 출력

local same = actor == obj -- UUID가 같으면 true
tostring(actor)           -- "Actor#123" (UUID가 123인 경우)

actor:Destroy()		-- 액터 파괴 예약 (즉시 파괴되지 않음)
actor:HasComponent("UBoxComponent") -- 등록된 컴포넌트 검사. 파생 클래스도 포함
local others = actor:GetOverlappingActors() -- 기존 겹침 목록. 액터 중복 없는 배열
```

`GetOverlappingActors()`는 `UWorld::UpdateOverlaps`가 관리하는 결과를 반환합니다.
양쪽에 Overlap 이벤트가 켜진 충돌 도형(`UShapeComponent`)이 필요합니다.
일반 표시용 메시만 붙이면 이 Overlap 검사에 참여하지 않습니다.

### LevelSequenceActor 재생 제어

```lua
local sequence = GetActorByName("ClearSequenceActor")
sequence:PlaySequence()
sequence:PauseSequence() -- 현재 카메라와 포즈 유지
sequence:StopSequence()  -- 카메라 복귀 및 완료 모드에 따른 상태 복구
local playing = sequence:IsSequencePlaying()
local moved = sequence:SeekSequence(11.5) -- 초 단위, 도착 지점을 즉시 평가
```

`SeekSequence(seconds)`는 재생 범위 안으로 시간을 제한하고, 건너뛴 구간의 이벤트는 실행하지 않습니다.
재생 중 호출하면 재생을 계속하므로 구도를 고정하려면 이어서 `PauseSequence()`를 호출합니다.
유효한 시퀀스 액터/에셋이 없거나 시간이 NaN·무한대이면 `false`, 평가 요청이 성공하면 `true`를 반환합니다.
`bPauseAtEnd=true`인 시퀀스의 자연 종료도 `IsSequencePlaying()`이 `false`가 되는 것으로 확인할 수 있습니다.

### Input

`RegisterInput`에 등록된 전역 함수입니다. 객체 생성 없이 직접 호출합니다.

#### 함수
```lua
IsKeyDown("w")         -- 현재 키를 누르고 있으면 true
IsKeyPressed("space")  -- 이번 프레임에 키를 누르기 시작했으면 true
IsKeyReleased("space") -- 이번 프레임에 키를 떼었으면 true

GetDeltaTime() -- 프레임 사이의 경과 시간 (초, number)
GetTime()      -- 엔진 타이머의 누적 경과 시간 (초, number)
```

#### 지원하는 키
```lua
-- 알파벳: "a" ~ "z"
-- 숫자: "0" ~ "9"
-- 특수 키:
-- "space", "enter", "escape", "tab"
-- "shift", "backspace", "ctrl", "alt"
-- "left", "right", "up", "down"

-- 키 이름은 대소문자를 구분하지 않음
IsKeyDown("w")
IsKeyDown("W")

-- 지원하지 않는 키 이름을 전달하면 오류 발생
-- IsKeyDown("invalid") -- Unknown key name: invalid
```

#### 사용 예시
```lua
function Tick(dt)
    -- W를 누르는 동안 X축으로 초당 100만큼 이동
    if IsKeyDown("w") then
        obj.Location = obj.Location + FVector(100, 0, 0) * dt
    end

    -- Space를 누르기 시작한 프레임에 실행
    if IsKeyPressed("space") then
        print("Space pressed")
    end

    -- Space를 뗀 프레임에 실행
    if IsKeyReleased("space") then
        print("Space released")
    end
end
```

`Tick(dt)`에서는 전달받은 `dt`를 사용합니다. `GetDeltaTime()`은 코루틴처럼 `dt`를 직접 전달받지 못하는 곳에서 사용합니다.
