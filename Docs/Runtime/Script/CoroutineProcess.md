## 코루틴 함수 실행 순서

### 코루틴 등록
1. Lua에서 `StartCoroutine` 함수 호출
1. `LuaBindings.cpp::RegisterCoroutine()`에 등록 되어있는 람다 함수 실행
1. `FCoroutineScheduler`에 코루틴 등록
	1. 현재 Tick 진행 중이면 Pending list에 등록
	1. Tick 진행 중이 아니면 바로 Coroutines list에 등록


### 코루틴 Tick
1. `FCoroutineScheduler::Tick()`에서 등록된 코루틴을 순회하며 실행
1. Coroutine이 대기 상태이면 상태 업데이트 후 다음 코루틴으로 넘어감
	1. `wait()` : 대기 시간 감소
	1. `wait_frame()` : 다음 스케줄러 갱신까지 대기
	1. `wait_until()` : 조건 함수 호출 후 true 반환 시 대기 종료
	1. `wait_until_move_done()` : 이동 작업이 끝날 때까지 대기
1. 대기 상태가 아닌 경우(`bReady == true`) 코루틴 다시 실행
1. 종료 시 전체 순회가 끝나고 Coroutine 삭제
1. Pending list에 등록된 코루틴이 있으면 Coroutines list로 이동

### 코루틴 Resume
`lua_resume()` API를 통해 코루틴 재개
1. 완료 시 `Status == LUA_OK`, false 반환 (정상 종료)
1. 실행 중 yield 호출 시 `Status == LUA_YIELD`, return 값 확인
	1. `wait()` : `("time", 요청 대기 시간)`
	1. `wait_frame()` : `()`
	1. `wait_until()` : `("until", 조건 함수)`
	1. `wait_until_move_done()` : `("move")`
1. 각 yield 타입에 따라 Wait state와 추가 정보를 `FCoroutine`에 저장 후 return true (계속 실행)