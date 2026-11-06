### Coroutine

여러 프레임에 걸쳐 실행할 동작을 순서대로 작성할 수 있습니다. 대기 중에는 해당 코루틴만 멈추고, 게임은 계속 실행됩니다.

#### 함수
```lua
-- 함수를 코루틴으로 실행
-- 첫 대기 지점까지 즉시 실행됨
StartCoroutine(function()
    -- 실행할 내용
end)

-- 아래 대기 함수는 코루틴 내부에서 사용
wait(1.0)    -- 1초 동안 대기
wait_frame() -- 다음 스케줄러 갱신까지 대기

-- 조건 함수가 true를 반환할 때까지 대기
wait_until(function()
    return IsKeyDown("space")
end)

-- 현재 스크립트의 obj를 목표 월드 위치로 이동
-- 두 번째 인자는 초당 이동 거리이며, 양수를 사용
-- 이동을 시작하고 즉시 다음 줄로 진행
move_to(FVector(100, 0, 0), 50)

-- 현재 액터의 move_to 이동 작업이 끝날 때까지 대기
wait_until_move_done()
```

#### 사용 예시

시간을 기다린 뒤 다음 동작을 실행합니다.

```lua
function BeginPlay()
    StartCoroutine(function()
        print("시작")

        wait(2.0)
        print("2초 경과")

        wait_frame()
        print("다음 스케줄러 갱신")
    end)
end
```

키 입력을 기다린 뒤 목표 위치로 이동합니다.

```lua
function BeginPlay()
    StartCoroutine(function()
        print("Space를 누르면 이동합니다")

        wait_until(function()
            return IsKeyDown("space")
        end)

        move_to(FVector(300, 0, 0), 100)
        wait_until_move_done()

        print("이동 작업 종료")

        wait(1.0)

        move_to(FVector(0, 0, 0), 100)
        wait_until_move_done()

        print("복귀 작업 종료")
    end)
end
```

반복 동작은 반복문 안에 대기 함수를 넣어 작성합니다.

```lua
function BeginPlay()
    StartCoroutine(function()
        while obj:IsValid() do
            obj:PrintLocation()
            wait(1.0)
        end
    end)
end
```

#### 주의사항

- `wait`, `wait_frame`, `wait_until`, `wait_until_move_done`은 `StartCoroutine`으로 실행한 함수 안에서 사용합니다. 일반 `BeginPlay()`나 `Tick(dt)`에서 직접 호출하면 오류가 발생합니다.
- `wait_until`의 조건 함수는 매 스케줄러 갱신마다 평가됩니다. `true` 또는 `false`를 반환하고, 내부에서 대기 함수를 호출하지 않습니다.
- `move_to`는 대기 함수가 아닙니다. 이동이 끝난 후 다음 동작을 실행하려면 `wait_until_move_done()`을 함께 사용합니다.
- 같은 액터에 `move_to`를 다시 호출하면 기존 이동 작업의 목표 위치와 속도가 변경됩니다.
- `wait_until_move_done()`은 이동 작업이 없어지면 재개됩니다. 액터나 루트 컴포넌트가 없어 이동 작업이 제거된 경우에도 재개될 수 있으므로, 도착 성공을 보장하지는 않습니다.
- 무한 반복문에는 대기 함수를 넣어야 합니다. 대기 없이 반복하면 게임 실행이 멈출 수 있습니다.
- `Tick(dt)`에서 매번 `StartCoroutine`을 호출하면 매 프레임 새 코루틴이 생성됩니다. 한 번만 시작할 동작은 `BeginPlay()` 등에서 실행합니다.