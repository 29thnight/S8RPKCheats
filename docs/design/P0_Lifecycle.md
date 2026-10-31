# P0 시작·종료 순서와 소유권

기준: S8RPKCheats `068ca1d881edf870f7cf06f935cf50e286004af8`. 소스 호출 순서를 정적으로 추적했다. hook callback의 동시 실행 종료나 DLL 언로드 안전성을 실측한 결과는 아니다.

## 시작

1. hid/dinput8은 `DllMain` process attach에서 초기화 스레드를 생성한다. dxgi는 attach에서는 모듈 핸들만 저장하고 factory wrapper 최초 호출 시 시작한다. 세 진입점은 `g_hModule`을 저장하고 스레드 핸들을 생성 직후 닫는다. 프록시별 상세 export는 [프록시 조사](P0_Proxy_Origin.md)에 있다.
2. `Source.cpp:32`의 `T00LoadEarlyLogConfig`는 기존 조기 로그 설정을 읽고 로그 writer를 시작한 뒤 `bStratagemFiveEnabled`를 줄 단위로 읽는다. `Source_impl.inc` 안의 같은 함수명은 매크로로 이 wrapper에 치환된다.
3. `T00PrepareStratagemFiveUiBridge`는 조기 설정이 OFF면 실제 준비를 건너뛰고 ready를 true로 둔다. ON이면 실제 브리지 준비 결과를 ready로 저장한다. `T01SetStratagemFiveFeature`도 설정 OFF에서 enable 요청을 건너뛴다.
4. `Source_impl.inc:301`의 100회 `Sleep(100)`은 `T01StartupSleep`으로 치환된다. ready가 true가 되는 순간부터 100ms 대기는 생략된다. OFF이거나 첫 준비가 성공하면 루프 전체의 100ms 대기가 생략된다. 준비가 계속 실패하면 최대 100회 대기와 재시도를 수행하며, 그 뒤 보고를 켠 마지막 준비 호출을 한 번 더 한다. 이 호출의 반환값은 지역 `stratagemUiBridgeReady`에 대입하지 않지만 wrapper 내부의 준비 상태에는 반영한다. 마지막 호출에서도 성공할 수 있으므로 이 호출 자체를 실패 결과로 취급하지 않는다.
5. `Engine` 생성 후 `g_Running=true`, `InitCheats` 즉시 시도와 최대 60회의 별도 재시도 worker를 시작한다. 이후 D3D hook, 입력 API hook, ClientBGThread를 시작하고 MainThread는 메뉴 키를 30ms 간격으로 확인한다. 일반 설정 로드는 `Menu::Loops`의 gameBase 준비 후에 진행된다.

`g_Running`, `g_KillSwitch`는 `helper.h`의 atomic bool이다. 초기화 루프의 내부 변수와 다른 전역 상태가 같은 동기화 계약을 가진다는 뜻은 아니다.

## MainThread 정상 종료 경로

1. `g_Running=false` 후 ClientBGThread와 초기화 재시도 thread를 join한다.
2. 미발견 무장 watcher, 시나리오 설정, 기술 초기화, 도로 두 기능, 군주 보너스의 OFF 함수를 순서대로 호출한다.
3. `Shutdown(false)`는 `s_done`을 설정하고 전체 MinHook 비활성화와 `MH_Uninitialize`를 호출한 뒤 렌더 WndProc 복원·전체 비활성화, Hooking 전체 비활성화를 호출한다. `Engine` 소멸자의 `g_Hooking.reset()`에서도 `MH_Uninitialize`가 호출된다. 반환 상태 확인은 이 호출 경로에 없다.
4. `g_Engine.reset()`이 Hooking과 RenderManager를 파괴한다. 이후 `FreeLibraryAndExitThread` 표기는 `Source.cpp` 매크로로 `T05FreeLibraryAndExitThread`에 치환된다. 이 wrapper는 디버그 스캐너를 중지·join하고 파일 로그 writer를 중지·join한 뒤 실제 `FreeLibraryAndExitThread`를 호출한다.

위 순서는 코드가 호출하는 순서다. 예를 들어 정상 종료의 여섯 기능 OFF 목록에는 전기 캡처, 전투 유닛 캡처, 공명, 특수능력 자동배정 취소 호출이 직접 들어 있지 않다. 각 기능이 다른 경로에서 중지되는지와 해당 시점에 실행 중인 worker·직접 패치가 없는지는 별도 증거가 필요하다. `MH_ALL_HOOKS`는 MinHook 관리 대상이며 직접 설치한 code cave의 원본 복원을 뜻하지 않는다.

## DllMain detach 경로

각 프록시의 process detach는 `Shutdown(lpReserved != NULL)`을 호출한다. `isTerminating=true`는 전체 MinHook 비활성화와 WndProc 복원 후 반환하고 worker join이나 RenderManager 해제를 수행하지 않는다. `isTerminating=false`도 이 함수 자체는 MainThread의 join·기능별 OFF 목록·로그와 스캐너 wrapper를 호출하지 않는다. `Shutdown`은 첫 호출에서 `s_done=true`를 설정하므로 이후 호출은 바로 돌아간다. 외부에서 별도로 unload하는 경우와 MainThread 정상 종료가 겹칠 때의 실제 순서는 확인하지 않았다.

## 이관 회귀 조건

조기 설정 OFF·브리지 첫 성공·재시도 성공·전체 실패의 실제 대기, D3D 및 게임 준비 지연, MainThread 요청 종료와 외부 detach, 각 worker·Present·WndProc·직접 패치 실행 중 종료, MinHook과 직접 패치의 독립 소유, 로그 drain과 문자열 포인터의 외부 수명을 확인한다. 새 runtime은 종료 완료의 증거가 없는 callback과 작업을 해제 대상으로 가정하지 않는다.

## D3D·입력 hook 설치 실패 경로

`RenderManager::GetD3DContext`는 임시 창 생성에 실패하면 false를 반환하지만, 창 생성 후 `D3D11CreateDeviceAndSwapChain`이 실패해도 창을 지우고 true를 반환한다. `MethodsTable`은 초기값이 null이며 장치 생성 성공 분기에서만 할당된다. 따라서 실패 뒤 `HookD3D`가 `MethodsTable[IDXGI_PRESENT]`에 접근하는 경로가 있다. 성공 분기의 `calloc` 결과도 `memcpy` 전에 검사하지 않는다. 이는 코드상 경로이며 실제 실패 재현은 하지 않았다.

`HookD3D`는 `Hooking::CreateHook`의 반환값을 확인하지 않고 `bInit=true`, 시간 hook 설치, 성공 로그를 수행한다. `MainThread_Initialize`도 `HookD3D` 반환값을 확인하지 않고 진행한다. 이어 `oGetCursorPos`가 null일 때 여섯 user32 API의 hook 생성과 `MH_EnableHook(MH_ALL_HOOKS)`를 호출하지만 각각의 결과를 검사하거나 일부 실패를 롤백하지 않는다. 한 API의 원본 포인터 상태만으로 여섯 설치의 성공을 판단할 수 없다.

회귀 조건은 임시 창·장치·테이블 할당 실패, Present hook 생성·활성화 실패, 여섯 입력 API 각각의 실패와 부분 설치, 반복 초기화 및 종료다. 실제 hook이나 게임 실행은 수행하지 않았다.

## 친밀도 쓰기 진단의 직접 hook 수명

도시 무장 정보의 `친밀 쓰기 검증` 버튼은 주인공과 선택 무장의 친밀도 주소를 계산한 후 `ArmOfficerAffinityWriteProbe`를 호출한다. 이 함수는 `OfficerData_impl.inc:359`에서 게임의 저장 명령을 찾아 직접 code cave를 설치하고 예상 주소를 기록한다. cave는 해당 명령에 들어올 때 예상 주소와 비교해 일치하면 실제 저장 주소·값을 기록하고 예상 주소를 0으로 바꾼다. 이후 원래 저장 명령을 실행한다. 화면에서 결과를 소비해 현재 메모리 값과 비교하고 로그를 남긴다. 이는 해당 경로의 설명이며 캡처 전후의 게임 데이터를 실행해 확인한 것은 아니다.

한 번 캡처하면 예상 주소는 지워지지만 hook 자체는 설치 상태로 남는다. `DisarmOfficerAffinityWriteProbe`도 캡처 상태만 지우며 원본 명령 복원이나 cave 해제를 하지 않는다. 기준 자체 cpp/h/inc에서 이 disarm 함수의 호출은 찾지 못했고, `g_affinityWriteHookInstalled`의 참조와 대입을 확인한 범위에도 해제 경로는 없다. 실패한 최초 설치에서만 할당된 cave를 해제한다. 따라서 MainThread 정상 종료의 MinHook 해제는 이 직접 patch의 원본 복원을 대신하지 않는다.

cave는 캡처 대기 여부를 판단하기 위해 호출마다 DLL 내부 `g_affinityExpectedWriteAddr`의 주소를 역참조한다. DLL이 언로드될 때 해당 patch가 남으면 이후 게임의 해당 저장 명령이 더 이상 유효하지 않은 DLL 주소를 참조할 수 있다. 이 결론은 코드상 수명 분석이며 실제 종료나 충돌을 재현하지 않았다. `volatile` 변수는 작업 간 동기화 수단으로 취급하지 않는다.

이관 검증은 진단 arm·자동 disarm·재요청, 주소 불일치, 원본 명령 복원 실패, 게임 쓰기와 진단 상태 변경의 겹침, 기능 사용 후 정상 종료 및 언로드 중 callback 종료다.

## 기본 포인터 재검색과 전법 횟수 재로드

`MainThread_Initialize`는 첫 `InitCheats` 반환값을 검사하지 않고 재시도 worker를 만든다. 따라서 첫 호출에서 성공해도 worker는 적어도 한 번 더 호출할 수 있다. 디버그 메뉴의 `기본 포인터 검색 (초기화)` 버튼도 `debug_impl.inc:663`에서 같은 함수를 직접 호출하며 반환값을 사용하지 않는다. 시작 재시도 worker와 디버그 메뉴 호출의 실행 순서는 소스만으로 확정되지 않는다.

재시도 worker는 각 `InitCheats` 호출 전과 100ms 대기 구간 사이에 실행·종료 flag를 확인한다. 그러나 `InitCheats` 내부의 이미지 패턴 검색 `FindPattern`에는 종료 flag 검사가 없다. 정상 종료의 join이 이 검색과 겹치면 검색 함수가 반환된 뒤에야 worker가 종료 조건을 다시 확인하고 join을 마칠 수 있다. 첫 MainThread 직접 호출과 디버그 메뉴에서의 호출도 같은 검색 함수를 사용한다. 검색 소요 시간과 실제 종료 대기는 측정하지 않았다.

현재 설치된 `SAN8RPK.exe` 파일 전체에서 초기 gameBase 검색 패턴 `4C 8B 05 ?? ?? ?? ?? 41 0F B7`을 각 `??`를 한 바이트로 하여 찾았을 때 일치는 0건이었다. 파일 SHA-256은 `000D450345728C23BEBCA581AB37B8BCF25266172767B5E6DEFB10A72A8C9457`이다. `InitCheats`의 실제 검색 범위는 로드된 모듈의 `SizeOfImage`이며, 파일과 실행 메모리의 바이트가 같다는 근거가 없으므로 이 결과로 초기화 성공 여부를 판단하지 않는다.

`InitCheats`는 모듈 정보 조회 성공 후 패턴 검색을 수행하고, 찾았을 때만 `s_gameBasePtrAddr`를 갱신한다. 검색 실패 시 기존 값을 지우지 않으므로 이전 주소가 남아 있으면 반환값은 여전히 true일 수 있다. 이어 검색 성공 여부와 관계없이 `LoadSkillCounts`를 호출한다. 이 함수는 파일이 열리면 기존 맵을 비우고 파일을 읽은 뒤 변경·저장 버전을 0으로 만든다. 따라서 저장 전 편집이 있으면 재로드로 파일의 이전 내용이 메모리에 다시 들어올 수 있다. 파일이 없으면 맵은 유지하지만 두 버전을 0으로 만들므로, 기존 미저장 변경이 별도 신규 변경이나 명시 저장이 없는 한 이후 지연 저장 대상에서 빠질 수 있다. 파일 열기에 실패하면 맵과 버전을 그대로 둔다.

맵과 파일 저장·로드는 각각의 mutex로 직렬화하지만, 기본 포인터 변수 `s_gameBasePtrAddr`는 일반 정수이며 재검색 대입과 `GetGameBase`/`GetGameBaseFast` 읽기 사이 공통 동기화는 이 코드에서 보이지 않는다. 이는 가능한 동시 접근 경로에 대한 정적 분석이며 실제 호출 겹침은 확인하지 않았다. 이관 시 초기화 결과, 재검색과 파일 재로드의 결합, 미저장 변경 보존, 이전 주소 보유, 시작 worker와 UI·background의 실행 순서를 각각 확인한다.

## 공통 게임 주소 조회와 공유 캐시

`Cheats.cpp:122`의 `GetGameBaseFast`는 기준 포인터 주소가 0이면 0을 반환하고, 아니면 포인터를 한 번 읽는다. 읽기 예외는 0으로 처리하지만 읽은 대상 객체의 접근 가능성은 확인하지 않는다. 반면 `GetGameBase`는 300ms 캐시와 `IsValidPtr` 검사를 사용하되, 해당 검사보다 먼저 `s_gameBasePtrAddr`를 역참조한다. 이 최초 읽기를 감싸는 예외 처리도 이 함수에는 없다. 따라서 기준 포인터 주소가 더 이상 읽을 수 없는 상태라면 뒤의 검사보다 먼저 읽기 예외가 발생할 수 있다.

일반 조회는 최초로 읽은 값이 이전 캐시와 같고 마지막 검사 후 300ms 미만이면 추가 검사 없이 반환한다. 그 외에는 기준 포인터의 8바이트를 검사하고 다시 읽은 뒤, 대상 주소의 첫 8바이트를 검사해 캐시에 넣는다. 이것은 이후에 사용하는 `gameBase+0xE0` 등 전체 객체의 유효성이나 같은 주소를 재사용한 새 게임 세대를 보증하지 않는다. 주소가 같은 채 객체 내용만 바뀌면 300ms 내 캐시 분기에서는 이를 별도로 식별하지 않는다.

`Menu::Loops`의 ClientBGThread 경로와 메뉴 렌더 경로가 둘 다 일반 조회를 호출하며, 함수 내부 캐시와 마지막 검사 시각은 일반 정적 변수다. 이 변수들의 읽기·쓰기를 감싸는 공통 잠금은 함수에 없다. 초기화 worker가 쓰는 기준 포인터도 일반 정수다. 호출이 실제로 동시에 겹치는지는 실행하지 않아 확인되지 않았다. 빠른 조회의 사용처에는 별도 기재 객체 수집 wrapper 등이 있으므로 두 조회의 현재 동작을 혼동하지 않는다.

공통 `IsValidPtr`는 범위 시작과 필요 시 끝이 `MEM_COMMIT`인지, `PAGE_NOACCESS`·`PAGE_GUARD`가 아닌지만 검사한다. 읽기·쓰기·실행 권한을 요청한 연산에 맞춰 구분하는 검사는 아니므로, 예를 들어 읽기 전용 영역도 이 조건을 통과할 수 있다. 범위가 여러 중간 메모리 영역에 걸칠 때 시작·끝 사이 모든 영역을 순회하지 않으며, 검사 이후에도 메모리 상태가 고정되지는 않는다. 회귀 조건은 기준 포인터 해제·재설정, 캐시 300ms 경계, 동일 주소의 다른 게임 세대, 렌더와 background 동시 조회, 조회와 재검색의 겹침, 다중 영역에 걸친 범위 및 검사 후 객체 교체다.

## MinHook 초기화와 전체 활성화의 범위

`Engine::Engine`은 `Hooking` 생성자에서 `MH_Initialize`를 호출하지만 반환 상태를 확인하지 않는다. 기재 runtime 네 설치 경로와 전기 허용 목록 설치는 `MH_OK`뿐 아니라 `MH_ERROR_ALREADY_INITIALIZED`도 허용한다. 반면 호출처를 찾지 못한 주인공 훅 후보는 `MH_OK`만 허용한다. 이 차이는 각 함수의 조건이며 동일한 실행 순서에서 모든 기능이 설치된다는 뜻은 아니다.

`Hooking::CreateHook`은 `MH_CreateHook` 성공 후 대상별 `MH_EnableHook`을 호출한다. 대상별 활성화가 실패하면 false를 반환하지만 앞에서 만든 등록 항목을 제거하지 않는다. `RenderManager::HookD3D`는 이 반환값을 확인하지 않고 `bInit`과 성공 로그를 설정한다. 이후 MainThread는 `g_Hooking->Initialize()`를 호출하고, 입력 API 등록 후에도 `MH_EnableHook(MH_ALL_HOOKS)`를 호출한다. MinHook 구현에서 전체 활성화는 그 라이브러리의 등록된 hook들을 대상으로 한다. 따라서 개별 활성화에 실패해도 등록이 남아 있다면 뒤의 전체 활성화가 그 항목을 다시 활성화하려는 코드 순서다. 실제 재시도 성공 여부는 반환값을 확인하지 않으며 실행하지 않았다.

전체 활성화·비활성화는 각 기능의 상태 flag나 소유권을 기준으로 대상을 선택하지 않는다. 기재 getter, 입력, 렌더와 시간 hook처럼 MinHook에 등록한 항목의 상태와 각 기능이 기록한 내부 상태를 함께 대조해야 한다. 직접 점프와 코드 공간 패치는 MinHook 전체 호출 대상이 아니다. 종료의 전체 비활성화·uninitialize와 직접 패치의 별도 복구는 앞 절의 순서를 따른다.

회귀 조건은 이미 초기화된 상태, 개별 생성 성공·활성화 실패 뒤 전체 활성화, 여섯 입력 중 일부 등록 실패, 전체 활성화 실패, 내부 적용 flag와 실제 활성 상태, 직접 패치와 함께 종료하는 경우다. 실제 hook 생성·게임 실행은 수행하지 않았다.

## 종료 요청의 현재 연결 상태

기준 자체 cpp·h·inc의 `g_Running`과 `g_KillSwitch` 참조를 대조했다. MainThread는 두 값을 각각 false·true가 아닌 실행 상태로 설정하고, `ClientBGThread`는 kill switch가 true면 running을 false로 바꾼다. `Shutdown`은 두 값을 false·true로 설정한다. MainThread의 End 키 종료 분기는 전체가 주석이므로 현재 활성 입력 경로가 아니다. 검색한 자체 소스에는 그 밖에 이 두 변수에 종료 요청을 대입하거나 별도 atomic 변경 함수를 호출하는 메뉴·단축키 경로를 찾지 못했다.

`Shutdown`의 자체 호출처는 MainThread의 종료 정리와 세 프록시의 `DLL_PROCESS_DETACH`다. 따라서 앞서 기술한 MainThread join·기능별 OFF·로그 종료는 **해당 루프가 종료된 경우의 코드 순서**이며, 사용자가 메뉴나 End 키로 그 순서를 시작할 수 있다는 확인 결과가 아니다. process detach가 `Shutdown`을 호출할 때는 loader 상태와 모듈 참조 수명이 별도로 작용하므로, 그 호출만으로 MainThread가 안전하게 정리와 `FreeLibraryAndExitThread`까지 수행했다고 판단하지 않는다. 외부 코드가 상태를 변경하거나 다른 방식으로 종료를 요청하는 가능성은 이 자체 소스 검색 범위 밖이다.

이관 확인 항목은 명시적 정상 종료 요청의 진입점, 새 작업 수락 중지, worker·render callback 완료, 직접 패치 복원, 로그 모듈 참조와 외부 unload·프로세스 종료의 차이다. 종료 또는 게임을 실행하지 않았다.

## 별도 `WriteBytes` 패치와 정상 종료의 연결 대조

`Cheats` 하위의 별도 `WriteBytes` 정의 14개를 각 기능의 setter·상위 묶음과 연결하고 `Source_impl.inc:397-402`의 정상 종료 OFF 호출과 대조했다. 아래의 ‘직접 호출 없음’은 확인한 정상 종료 코드와 기준 자체 cpp·h·inc의 호출 검색에 대한 결과이며, 이미 사용자가 OFF했는지 또는 게임 코드에 실제 패치가 남았는지는 뜻하지 않는다.

| 기능·패치 소스 | 적용과 OFF의 상위 경로 | MainThread 정상 종료에서의 연결 |
|---|---|---|
| 내정 포상 `DomesticRewardCondition.cpp` | 메뉴의 `SetDomesticRewardMode` | 해당 setter 직접 호출 없음. 설정으로 보관하지 않는 별도 UI 기능. |
| 임무 강제 배정 제외 `MissionCpuHeroExclusion.cpp` | 설정 적용 또는 메뉴의 `SetMissionCpuHeroExclusion` | 해당 setter 직접 호출 없음. |
| 포로 관리 3개 `DeputyCaptureFix.cpp`, `IsolatedCityCaptureFix.cpp`, `AIExecutionConditionChange.cpp` | `SetPrisonerCaptureManagement`가 세 패치를 묶어 설치·해제 | 이 묶음의 OFF 직접 호출 없음. 개별 설치 실패의 롤백은 정상 종료와 별개. |
| AI 전투 2개 `AIRefusalWarFix.cpp`, `AIHighHonorSurrenderFix.cpp` | `SetAIWarImprove`가 개별 hook과 정적 패치를 묶음 | `SetAIWarImprove(false)` 직접 호출 없음. 같은 `Roadblock.cpp`의 `SetRoadBlock(false)`와 `SetRoadBlock2(false)`는 별도 도로 worker·데이터 경로다. |
| 평정 재야 처단 `CouncilExecuteFreeOfficers.cpp` | 설정 또는 메뉴의 `SetCouncilExecuteFreeOfficers` | 해당 setter 직접 호출 없음. |
| 도독 포로 처분 `GovernorPrisonerDisposal.cpp` | 설정 또는 메뉴의 `SetGovernorPrisonerDisposal` | 해당 setter 직접 호출 없음. |
| 단절 영토 이동 `IsolatedTerritoryMovement.h` | 설정 또는 메뉴 → `SetIsolatedTerritoryMovementFeature` → 내부 setter | 이 wrapper의 OFF 직접 호출 없음. |
| 원군 즉시 행동 `ReinforcementArrivalAction.cpp` | 설정 또는 메뉴의 `SetReinforcementArrivalAction` | 해당 setter 직접 호출 없음. |
| 수비 원군 배치 `ReinforcementDefenderPlacement.cpp` | 설정 또는 메뉴의 `SetReinforcementDefenderPlacement` | 해당 setter 직접 호출 없음. |
| 군주 이동 제안 `RulerTransferProposal.cpp` | 전역 생성자의 설정 로드 또는 메뉴의 `SetRulerTransferProposalMode` | 이 setter의 OFF 직접 호출 없음. |
| 병력수 공방 `TroopCountCombatScaling.cpp` | 설정 또는 메뉴의 `SetTroopCountCombatScaling` | 해당 setter 직접 호출 없음. |

`Shutdown(false)`의 MinHook 일괄 해제는 이 표의 직접 코드·테이블 수정에 대한 기능별 OFF 호출이 아니다. 각 기능의 원본 검증, 부분 복구, cave 보존 여부는 [패치 조사](P0_War_Patches.md)와 [기능 보존표](P0_Feature_Contracts.md)를 따른다. callback 실행 중지와 실제 언로드 결과는 소스 대조만으로 확정하지 않았다.
## MinHook의 스레드 정지와 개별 해제

포함된 `Hooking/hook.c`의 MinHook은 별도 worker를 만드는 대신 hook 활성·비활성화 호출 스레드에서 현재 프로세스의 다른 스레드를 snapshot으로 열거한다. `Freeze`는 각 스레드를 열어 `SuspendThread`를 호출하고, hook 주소나 trampoline의 명령 위치를 필요에 따라 변환한다. 코드 쓰기 뒤 `Unfreeze`는 스레드를 다시 열어 `ResumeThread`를 호출한다. snapshot 생성·목록 할당·스레드 열기·정지·컨텍스트 읽기/쓰기·재개가 모두 성공했는지를 호출자에게 반환하는 구조는 아니다. snapshot 이후 생긴 스레드나 callback 전체 완료도 이 목록만으로 보장하지 않는다.

개별 대상 `MH_DisableHook(target)`은 `EnableHook(target, false)`로 들어가지만 `hook.c:696`의 `Freeze`에 `ACTION_ENABLE`을 넘긴다. 이때 이미 활성인 hook은 `ProcessThreadIPs`에서 현재 활성 상태와 요청 action의 활성 상태가 같다고 판단해 명령 위치 변환을 건너뛴다. 그 뒤 `EnableHookLL`은 실제로 false를 전달받아 대상 코드를 복원한다. 반면 `MH_DisableHook(MH_ALL_HOOKS)`의 전체 해제와 활성 상태의 `MH_RemoveHook`은 각각 `ACTION_DISABLE`을 사용한다. 같은 동작은 비교한 공식 MinHook v1.3.3 파일에도 있다.

이 차이는 개별 해제 시 trampoline에서 실행 중인 스레드의 명령 위치가 그 해제 호출에서 원본 위치로 조정되지 않는 코드 경로를 뜻한다. 또한 개별 해제로 이미 비활성화한 후 `MH_RemoveHook`을 호출하면 `RemoveHook`은 활성 hook에 대한 `Freeze` 분기를 건너뛰고 trampoline을 해제한다. 전기 허용 hook의 부분 설치 실패 복구는 개별 `MH_DisableHook` 성공 또는 이미 비활성 상태 뒤 `MH_RemoveHook`을 호출하고, 배속 설치 실패 복구도 두 함수를 순서대로 호출한다. 실행 중인 callback이나 trampoline이 실제로 겹치는지는 확인하지 않았다.

이관 검증은 개별·전체 해제, 활성 상태 직접 제거와 비활성 후 제거, trampoline 안에 있는 스레드, 열거·정지·컨텍스트 처리 실패, 새 스레드 생성과 종료다. 포함 라이브러리와 게임은 실행하지 않았다.
### 전체 MinHook 해제와 진행 중 입력 callback

일반 `Shutdown(false)`는 `MH_DisableHook(MH_ALL_HOOKS)` 다음 `MH_Uninitialize()`를 호출한다. 전체 해제는 등록된 hook을 비활성화하면서 앞 절의 스레드 정지·명령 위치 변환을 수행한 후 스레드를 재개한다. 이어 `MH_Uninitialize`는 `EnableAllHooksLL(false)`를 다시 호출하고, 성공하면 `UninitializeBuffer`에서 trampoline을 포함한 실행 메모리 블록을 해제한다. 이미 모든 hook이 비활성이라면 두 번째 전체 해제는 변경할 항목이 없어 `Freeze`를 다시 호출하지 않는다.

예를 들어 `Source_impl.inc`의 입력 callback `hkGetAsyncKeyState`는 조건에 따라 `oGetAsyncKeyState`를 호출하고, 이 포인터는 `MH_CreateHookApi`가 전달하는 원본 trampoline이다. 전체 해제 전에 callback 본문에 들어온 스레드가 본문에서 계속 진행 중이면, hook 입구를 복원하는 것만으로 이 callback이 `oGetAsyncKeyState` 호출을 마쳤는지는 확인되지 않는다. MinHook의 명령 위치 변환은 등록된 원본·trampoline의 지정된 명령 위치를 대상으로 하며 callback 본문에 들어간 호출의 완료를 기다리는 카운터가 아니다. 따라서 종료 시 callback 완료와 trampoline 해제 순서를 별도로 보장해야 한다는 이관 근거가 된다.

실제 종료와 입력 callback의 겹침은 실행하지 않았다. process 종료용 `Shutdown(true)`는 `MH_Uninitialize`를 호출하지 않는 별도 경로다.
### DllMain detach의 로더 잠금과 MinHook 잠금

세 프록시의 `DllMain`은 `DLL_PROCESS_DETACH`에서 `Shutdown(lpReserved != NULL)`을 직접 호출한다. `Shutdown`이 이미 완료 표시됐다면 바로 반환하지만, 처음 진입한 경우 프로세스 종료·일반 unload 두 분기 모두 `MH_DisableHook(MH_ALL_HOOKS)`를 호출한다. MinHook의 이 함수는 내부 spin lock을 얻은 뒤 필요한 경우 다른 스레드를 열거·일시 정지한다. `Shutdown`의 `s_done`은 일반 bool이며 두 호출을 원자적으로 직렬화하는 잠금은 이 함수에 없다.

Microsoft는 `DllMain`이 로더 잠금을 보유한 상태에서 호출되고, 그 안에서 다른 스레드와 동기화하거나 로더 잠금을 기다리는 코드가 가진 잠금을 얻으면 교착이 생길 수 있다고 설명한다([DLL 모범 사례](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)). 따라서 이 경로는 로더 잠금을 보유한 상태에서 MinHook 내부 잠금과 스레드 정지 절차를 수행하는 코드로 분류한다. 실제로 다른 스레드가 어떤 잠금을 보유하고 있었는지, 교착이 발생하는지는 조사에서 실행하지 않았다. 두 분기의 WndProc 복원도 `DllMain` 호출 중 이루어질 수 있으며, 정상적인 외부 정리 진입점을 먼저 수행한 경우와 구분해야 한다.
`InitCheats`의 최대 60회 재시도는 게임 포인터 초기화 함수에 한정된다. `HookD3D`의 기준 소스상 호출은 시작 시 한 곳이며, 반환값을 검사하지 않고 후속 초기화로 진행한다. 따라서 D3D context를 얻지 못했거나 Present의 `MH_CreateHook` 자체가 실패한 경우, 이 재시도 worker가 Present 등록을 다시 만드는 경로는 없다. 생성은 성공하고 활성화만 실패한 경우에는 위 전체 활성화가 남은 등록에 적용되는 별도 경로다. 동적 호출·실행 결과는 확인하지 않았다.

`Fastrelationship.cpp`의 즉시 경애는 위 14개 `WriteBytes` 정의와 다른 `PatchBytes`를 사용하는 직접 패치다. 기준 자체 cpp·h·inc에서 `SetFastRelationship` 참조는 정의·선언, 설정 테이블의 setter, 메뉴 호출이며, `MainThread`의 정상 종료 OFF 목록에는 없다. `Shutdown`의 MinHook 일괄 비활성화는 이 직접 바이트 쓰기를 복구하는 호출이 아니다. 따라서 기능이 켜진 채 정상 종료로 진입하는 경우 이 종료 경로 자체에는 저장된 원본을 되쓰는 단계가 없다. 앞서 사용자가 OFF했는지와 실제 메모리 상태는 이 정적 검색만으로 알 수 없다. setter의 복구 실패 처리는 [기능 보존표](P0_Feature_Contracts.md#즉시-경애-코드-패치의-상태와-복구)에 기록했다.

## 전기 필터와 데이터 복원의 종료 경계

정상 MainThread 종료의 여섯 기능 OFF 목록과 `Shutdown(false)` 호출 경로에서 전기 기간·쿨다운의 원본을 일괄 복원하는 호출은 찾지 못했다. 기간 원본은 `TickTengiListDurations`의 설정 OFF 처리에서, 쿨다운 원본은 해당 후보의 게임 판정 경로에서 각각 복원한다. 전기 필터의 세 detour는 MinHook 전체 해제의 대상이지만, 이것이 이미 수정된 기간·쿨다운 데이터의 복원을 수행하는 것은 아니다. 직접 설치된 전기 캡처 cave는 별개이며 정상 종료 목록에 `SetTengiCapture(false)`가 없다. 기준 자체 cpp·h·inc의 해당 이름 직접 검색에서도 OFF 호출은 찾지 못했고, 확인된 호출은 설정 지연 적용과 `TengiCave_Tick`의 ON뿐이다. 이는 간접·외부 호출을 배제하지 않는다. 세 경로의 상세 조건은 [전기 조사](P0_Tengi_Ronin.md)를 따른다. 실제 unload와 게임 데이터 잔존은 실행하지 않았다. `TengiCave.cpp`에서 세 원본 trampoline 포인터는 설치 중 대입되고 `s_allowedFilterReady`는 전체 활성화 후 true로 게시된다. 확인한 코드에는 정상 `Shutdown`에서 이 상태를 내리거나 진행 중 detour의 완료를 기다리는 호출이 없다. 전체 비활성화·`MH_Uninitialize`와 이미 진입한 detour가 겹치면 detour가 원본 trampoline을 계속 호출하는 시점과 그 메모리 수명을 별도로 검증해야 한다. 실제 겹침은 확인하지 않았다.