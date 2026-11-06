# 빙판 이동

`USlidingMovementComponent`는 XY 평면에서 움직인다. 입력을 누르면 최대 속력까지 가속한다.
일반 바닥에서는 입력을 놓으면 감속해서 멈추며, `UIceVolumeComponent` 안에서는 관성으로 계속 미끄러진다.
빙판에서는 조향 방향과 실제 이동 방향을 분리한다. 조향이 먼저 바뀌고 실제 속도 방향이
접지력에 따라 뒤따라가므로 코너에서 옆으로 밀리는 드리프트가 생긴다.
벽과 충돌하면 이동을 중단하고 `OnDeath`를 한 번 발행한다. 중력/점프/강체 반응은 포함하지 않는다.

## 에디터 설정

1. 플레이어 액터에 **Add Component → USlidingMovementComponent**를 추가한다.
   기존 위치 변경 스크립트나 다른 이동 컴포넌트와 함께 사용하지 않는다.
   플레이어가 `APawn`이면 기본 PlayerController가 이 Pawn을 선택한다.
   `ADefaultPawn`은 별도의 직접 이동 로직이 있으므로 사용하지 않는다.
2. 플레이어에 **USphereComponent**를 추가하고 루트에 붙인다. 상대 위치는 0으로 두고
   `bGenerateOverlapEvents=true`, `bBlockComponent=false`로 설정한다.
   `SphereRadius`와 이동 컴포넌트의 `CollisionRadius`를 같은 크기로 맞춘다
   (기본 0.5, 콜라이더 월드 스케일 1 기준). 전자는 Overlap, 후자는 벽 Sweep에 사용된다.
   표시용 플레이어 메시의 `bGenerateOverlapEvents`는 끈다.
   플레이어 루트가 이동하며 Z 높이는 유지된다. 초기 입력 전에는 정지한다.
3. 플레이어의 `ScriptPath`를 `Scripts/ice_player.lua`로 지정한다.
4. 빙판 액터에 **UIceVolumeComponent**를 추가한다. `BoxExtent`와 Transform으로
   플레이어 높이를 포함하는 영역을 잡는다. 표시용 바닥 메시의 `bBlockComponent`는 끈다.
   빙판 영역의 `bGenerateOverlapEvents`를 켠다.
   `UWorld::UpdateOverlaps → FCollision::Overlap → Begin/EndComponentOverlap → Lua` 경로로
   플레이어의 `OnBeginOverlap` / `OnEndOverlap`이 호출된다. 샘플 Lua는 기존 겹침 목록에서
   `UIceVolumeComponent`가 있는 액터를 찾아 `SetOnIce`를 호출한다.
   한 액터의 여러 영역이나 여러 빙판 액터 중 하나라도 겹쳐 있으면 빙판이다.
   이동 컴포넌트가 빙판을 직접 순회하거나 `SphereBox`를 별도로 호출하지 않는다.
5. 벽의 **UBoxComponent 또는 UStaticMeshComponent에서 bBlockComponent를 켠다.**
   벽 액터와 플레이어의 `bActorEnableCollision`도 켠다.
   트리거/바닥/빙판 영역의 `bBlockComponent`는 끈다.

## 가속과 드리프트 설정

| 속성 | 기본값 | 역할 |
|---|---|---|
| MoveSpeed | 6 | 최대 속력. 대각선 입력도 이 속력을 넘지 않는다. |
| Acceleration | 8 | 초당 속력 증가량. 정지 상태에서 최대 속력까지 약 0.75초. |
| Deceleration | 18 | 일반 바닥에서 입력을 놓았을 때의 감속도. 빙판에는 적용하지 않는다. |
| TurnRate | 180 | 초당 조향 방향 변경 각도. 실제 이동 방향은 IceGrip에 따라 늦게 따라온다. |
| IceGrip | 1.5 | 빙판 접지력(1/초). 낮으면 크게 미끄러지고, 높으면 조향을 빨리 따라간다. |

강한 드리프트는 `IceGrip=0.5`, 빠른 방향 전환은 `IceGrip=4`부터 비교한다.
`IceGrip=0`이면 이미 움직이는 물체의 실제 이동 방향이 바뀌지 않는다.
빙판에서 입력을 놓으면 속력이 유지된다. 남아 있는 조향 차이는 접지력에 따라 서서히 해소된다.
반대 방향 입력도 가속과 선회로 해석해 기존의 “빙판에서는 멈출 수 없음”을 유지한다.
이것은 속도와 조향을 분리한 아케이드 이동이며 타이어/질량 등의 차량 물리는 사용하지 않는다.
조향은 내부 이동 상태이며 액터 루트와 자식 카메라의 회전은 변경하지 않는다.
사망/부활 시 속도와 입력을 비우며, 부활할 때 조향 상태도 초기화한다.

## Lua

```lua
obj:SetMoveInput(FVector(1, 0, 0)) -- 매 Tick 호출. 입력이 없으면 FVector(0, 0, 0)
obj:SetOnIce(true)                -- Lua가 Overlap 결과에 따라 이동 모드를 설정
local onIce = obj.IsOnIce          -- 읽기 전용
local dead = obj.IsDead            -- 읽기 전용
obj:Respawn(FVector(0, 0, 1))      -- 위치 변경, 속도/입력/사망 상태 초기화
obj:Respawn()                      -- BeginPlay 때 저장한 출발 위치에서 부활

function OnDeath(wall)
    -- 이 시점에는 이미 IsDead=true이고 Velocity=0이다.
    -- 이펙트/부활/게임 오버 같은 게임 규칙을 여기서 처리한다.
end
```

위 API는 `USlidingMovementComponent`가 있는 액터에서만 사용할 수 있다.
범용 Actor API인 `obj:GetOverlappingActors()`는 기존 Overlap 목록의 액터를 중복 없는 Lua 배열로
반환하고, `other:HasComponent("UIceVolumeComponent")`는 등록된 컴포넌트의 종류를 확인한다.
이 API는 별도 충돌 검사를 수행하지 않는다. Lua가 진입/이탈 이벤트마다 전체 겹침 목록을 확인하므로
한 영역에서 나와도 다른 영역에 남아 있으면 빙판 상태를 유지한다.
핫 리로드 시 기존 겹침 목록으로 Lua 상태를 복구한다. 도형 등록 해제/삭제 시에는 양쪽 겹침 참조를
해제하고 EndOverlap을 전달한다.
Overlap 갱신은 월드의 모든 이동 Tick 뒤에 실행되므로 진입/이탈 결과는 다음 이동 Tick부터 적용된다.
부활 직후의 빙판 상태도 이 갱신에서 결정된다.
샘플 스크립트는 WASD 입력과 1초 뒤 시작 위치 부활을 제공한다.
R로 즉시 복귀할 수 있다. 부활 대기는 Lua Tick에서 관리해 수동 복귀 뒤 이전 타이머가 다시 부활시키지 않는다.
`obj.Location` 직접 변경이나 `move_to`는 충돌 이동을 우회하므로 이 이동과 섞지 않는다.

## C++ 이벤트와 수명

`Movement->OnDeath.AddDynamic(Listener, &TListener::HandleDeath)`로 구독한다.
함수 시그니처는 `void HandleDeath(AActor* Wall)`이다. 반환된 `FDelegateHandle`로 `Remove`하거나
`RemoveAll(Listener)`로 해제할 수 있다. `AddDynamic`은 UObject 생존과 UUID를 검사한다.
`Add`의 캡처 람다와 `AddRaw`는 구독자가 직접 수명을 관리해야 한다.

컴포넌트는 BeginPlay에서 소유 액터의 `NotifyDeath`를 AddDynamic으로 연결하고,
소멸 시 해당 핸들을 해제한다. 액터는 기존 Overlap/Hit와 같은 방식으로 현재 Script의
`OnDeath`를 호출한다. Lua 함수를 델리게이트에 캡처하지 않으므로 핫 리로드 뒤에도 오래된
환경이 남거나 바인딩이 누적되지 않는다. 사망 상태는 Broadcast 전에 설정되어 재진입을 막는다.
콜백에서 액터를 없앨 때는 기존 Lua `obj:Destroy()`의 프레임 말 지연 파괴를 사용한다.

## 충돌 범위

이 이동은 `SweepSingle(..., bBlockingOnly=true)`로 경로 전체를 검사한다.
표시되지 않는 콜라이더도 검사하며, 일반 투사체의 기존 Sweep 동작은 유지한다.
벽 판정은 기존 메시 Sweep과 같은 **반지름으로 확장한 월드 AABB**를 사용한다.
회전/곡면 메시에서는 보수적으로 일찍 충돌할 수 있으므로 벽은 축에 정렬된 Box로 배치한다.
일반 바닥에서도 벽 접촉은 사망이다. 빙판 진입/이탈은 기존 Overlap 이벤트로 판정한다.

## 테스트 씬

프로젝트 파일에 새 소스가 보이지 않으면 `SlnGenerator.bat`으로 갱신한 뒤 빌드한다.
에디터에서 `Scenes/IceSlide.scene`을 열고 Play를 누른 다음 게임 뷰포트를 클릭한다.
플레이어는 APawn이며 45도로 내려다보는 추적 카메라와 입력 스크립트가 연결되어 있다.
카메라 설정은 IcePlayer → PlayerCamera → Transform에서 바꾼다.
기본 상대 위치는 X=-15, Y=0, Z=15이고 회전은 Pitch=45, Yaw=0, Roll=0이다.
에디터 Rotation의 X/Y/Z는 각각 Roll/Pitch/Yaw이므로 **X=0, Y=45, Z=0**을 입력한다.
이 위치와 각도는 플레이어를 화면 중앙에 둔다. Play를 종료하고 씬을 다시 불러오면 파일 수정이 반영된다.

| 확인 | 조작 | 기대 결과 |
|---|---|---|
| 가속 | 출발 위치의 회색 바닥에서 W 누르기 | 정지 상태에서 약 0.75초에 걸쳐 최대 속력까지 빨라진다. |
| 일반 이동 | 회색 바닥에서 WASD를 눌렀다 놓기 | 감속해서 정지한다. 대각선도 같은 최대 속력이다. |
| 빙판 이동 | W로 파란 바닥에 진입한 다음 키 놓기 | 계속 전진한다. |
| 드리프트 | 빙판에서 W로 진행하다 D를 길게 누르기 | 기존 X축 속도가 즉시 사라지지 않고 옆으로 밀리며 Y축 방향으로 선회한다. |
| 역방향 조향 | 빙판에서 반대 방향 S 누르기 | 제동으로 멈추는 대신 돌아서 방향을 바꾼다. |
| 접지력 | IceGrip을 0.5와 4로 각각 설정하고 같은 코너 돌기 | 0.5에서 더 크게 미끄러지고, 4에서 더 빨리 방향을 잡는다. |
| 인접 빙판 | W로 파란 바닥 중앙을 통과하며 키 놓기 | 두 영역 경계에서도 미끄러짐이 끊기지 않는다. |
| Overlap 설정 | 빙판의 bGenerateOverlapEvents를 끄고 진입하기 | 해당 영역은 빙판 이동을 활성화하지 않는다. |
| 중첩 영역 | 두 영역을 겹치게 배치하고 하나에서만 벗어나기 | 나머지 영역을 벗어나기 전까지 빙판 상태가 유지된다. |
| 영역 제거 | 플레이어와 겹친 빙판 액터를 삭제하거나 도형 등록을 해제하기 | 남은 빙판이 없다면 일반 이동으로 복귀하고 오래된 도형 참조가 남지 않는다. |
| 빙판 이탈 | W로 파란 바닥을 지나 반대쪽 회색 바닥에 도달한 뒤 키 놓기 | 일반 감속이 적용되어 멈춘다. |
| 사망/부활 | 빨간 벽으로 이동하기 | 접촉 위치에서 정지하고 Output Log에 사망 로그 한 번, 1초 뒤 출발점 복귀. |
| 수동 복귀 | 사망 대기 중 R을 누른 뒤 다시 이동하기 | 즉시 복귀하며 이전 사망 타이머가 다시 위치를 초기화하지 않는다. |
| 얇은 벽 | MoveSpeed=60, Acceleration=600으로 바꾸고 빨간 벽으로 이동하기 | 벽을 통과하지 않고 사망한다. |
| 숨겨진 충돌 | Wall_Obstacle의 표시용 Wall 메시를 숨긴 뒤 해당 위치로 이동하기 | 별도의 WallCollider 박스가 계속 사망 판정한다. |
| PIE 수명 | Play/Stop을 반복하고 다시 벽에 닿기 | 매번 사망 이벤트가 한 번만 발생한다. |

빙판 두 영역은 IceFloor 아래 IceLeft/IceRight다. 벽 판정을 끄려면 해당 메시/도형의
`bBlockComponent`를 끈다. `Wall_Obstacle`은 메시가 아닌 별도의 Box 콜라이더를 사용한다.
테스트 후 MoveSpeed=6, Acceleration=8, IceGrip=1.5로 돌린다.
