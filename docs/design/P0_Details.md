# P0 세부 조사 기록

기준 커밋 `068ca1d881edf870f7cf06f935cf50e286004af8`, 정적 소스 조사. 경로는 참고 저장소 기준이다.

## 조사 색인과 한계

- [빌드 입력과 인용 include 목록](p0/include_inputs.csv): 프로젝트에 선언된 126개 컴파일 항목과 상위 props의 2개 항목에서 시작해 쌍따옴표 include를 재귀 탐색한 결과다. 조건부 전처리, 컴파일러 시스템 경로, 암시적 PCH, 매크로 include는 평가하지 않았다. 따라서 실제 전처리 결과와 같다고 단정하지 않는다.
- [매크로·스레드·훅·패치·파일·링크 위치](p0/source_locations.csv): 위 탐색으로 도달한 파일의 텍스트 검색 결과다. 주석, 조건부 코드, 라이브러리 내부 정의와 호출이 함께 포함되므로 각 줄의 문맥을 확인해야 한다. 직접 메모리 대입 및 다른 이름의 래퍼 호출은 이 검색만으로 완전하게 열거되지 않는다.
- [설정 코드 위치](p0/config_locations.csv)와 [기본 설정 항목](p0/config_entries.csv): 전자는 후보 줄 색인, 후자는 `ConfigBase.inc`의 `g_Entries` 항목과 확인된 초기화·적용 함수 위치다. 별도 수동 저장 필드, 동적 키와 초기화가 다른 곳에 있는 값은 원문 확인이 필요하다.

include 탐색에서 미해결 5개가 나왔다. `imgui_user.inl`과 `imgui_user.h`는 선택 매크로 아래에 있고 `stb_sprintf.h`, FreeType 헤더도 선택 구성 아래에 있다. `imstb_truetype.h`의 `stb_truetype.h` 포함은 샘플 코드 영역이다. 전처리 평가 없이 활성 여부를 확정하지 않는다.

## 설정과 세션

- `ConfigBase.inc:173`의 경로는 DLL 디렉터리의 `S8RPK_cheat_config.json`이다. `Source.cpp`의 조기 읽기는 책략 UI 설정을, `ConfigBase.inc:197`은 로그 설정을 줄 단위로 찾는다.
- `Menu_impl.inc:373`의 `Menu::Loops`는 게임 주소와 p1을 얻고, 게임 기반 주소가 준비되면 설정을 한 번 로드한다. p1 또는 gameBase가 바뀌거나 p1이 준비되지 않을 때 책략·전투 관련 세대 상태를 폐기한다.
- `ConfigBase.inc:735`는 최대 1초마다 적용 상태를 확인한다. gameBase와 p1의 최초 감지 후 각각 3초를 기다리고 `requiresP1`, `isWar` 및 전투 준비 여부에 따라 적용한다. gameBase 소실과 p1 소실에 대한 초기화 경로가 다르다.
- 기본 저장은 파일을 열어 전체 내용을 출력하고 `Config.cpp:246`의 wrapper는 추가 값을 키별로 다시 쓴다. 실패 보고 및 기존 파일 호환을 새 저장 경로에서 확인해야 한다. 동적 `tengiListAllowN`, `tengiListMonthsN`, `tengiListCooldownYearsN`도 보존 대상이다.

## 스레드와 종료: 확인한 소유 경로

| 구성 | 시작·소유 | 종료와 이관 시 확인점 |
|---|---|---|
| 초기화 / 백그라운드 | 각 `dllmain*.cpp`에서 MainThread를 만들고 핸들을 닫는다. `Source_impl.inc:331,366`에서 재시도와 ClientBGThread를 지역 `std::thread`로 소유한다. | `Source_impl.inc:389` 이후 두 스레드를 join한다. hook callback 자체의 종료 증거는 별도다. |
| 파일 로그 | `showlog.cpp:120`에서 생성한 핸들을 보관한다. | `ShutdownFileLogWriter`가 대기 후 핸들을 닫는다. Source의 정상 unload wrapper에서 호출한다. |
| 미발견 감시, 시작 설정, 기술 초기화, 도로, 군주 보너스 | 각 기능의 정적 핸들과 정지 이벤트를 보관한다. `Source_impl.inc:397` 이후 기능별 비활성화가 있다. | 해당 비활성화가 대기하고 핸들을 정리하는 경로를 기능별로 확인해야 한다. |
| 도시 변환 등 일회성 기능 | 예: `BangmokCity.cpp:44`는 worker 생성 직후 핸들을 닫고 atomic 실행 표시를 사용한다. | 닫은 핸들은 worker 종료 대기가 아니다. 정상 종료와의 관계를 기능별로 조사한다. |
| 기재 자동 배정·공명 | `SpecialAbilityAutoAssign.cpp:295,403`, `Resonancecave.cpp:203,256`은 detach를 사용한다. | 작업 완료와 DLL 수명 관계 확인이 필요하다. |
| 디버그 스캐너 | `debug.cpp`가 worker 목록을 보관하고 join 경로를 제공한다. `debug_impl.inc` 호출은 래퍼 매크로 영향을 확인해야 한다. | 보이는 detach 표기만으로 실행 시 detach라고 판단하지 않는다. |

스레드 색인은 검색 대상 표기만 포함한다. 각 callback의 실제 스레드, 재진입, 게임 객체 파괴와의 순서는 게임 실행에서 확인해야 한다.

## 리소스·출처

`Resources.rc`에는 범위 PNG 11개와 effect/speciality JSON 두 개가 있고, `Fonts.h`에는 폰트 바이트 배열이 있다. `malgun*.ttf` 세 파일의 선택 경로, 파일 해시와 이름 테이블 및 외부 JSON·INI의 우선순위와 생성 조건은 [자원 조사](P0_Resources.md)에 보강했다. 폰트·이미지의 실제 도입 출처와 실행 시 로딩 결과는 아직 확인하지 못했다.

기존 `Framework`에는 ImGui와 백엔드, `Hooking`에는 MinHook/HDE 구현이 포함되어 있다. `pch.h`에는 D3D11/D3D12/DXGI/XInput/ws2_32/wininet 링크 지정이 있다. 각 원본 버전과 수정분 및 원문 고지, 프록시 export별 ABI는 아직 확정하지 않았다.

## 추가로 확인한 빌드 입력과 조기 실행

- 상위 `Directory.Build.props`에서 추가하는 `RulerTransferProposalBootstrap.cpp`의 전역 객체 생성자는 `LoadRulerTransferProposalPreference()`를 호출한다. 일반 MainThread 초기화 이전의 정적 초기화 경로이므로, 프록시 진입점만 살펴보면 설정 읽기 순서를 놓친다. 같은 기능의 구현은 `bRulerTransferProposal` 키와 코드 패치를 사용한다.
- `Framework/imgui.h:30`에 표시된 버전은 `1.91.0 WIP`다. 원본 특정 커밋과 변경분은 아직 확인하지 않았다.
- `Engine.cpp:426`부터 DLL 기준 폰트 디렉터리의 `malgunbd.ttf`, `malgun.ttf`, 시스템 `C:\Windows\Fonts\malgun.ttf`, 내장 `RudaBold` 순서로 폰트를 찾는다. 실제 배포물에 각 파일이 필요한지는 경로와 로딩 결과를 추가로 확인한다.

## 훅·패치별 확인 사항

| 소유 코드 | 설치·실패·해제 경로 | 이관에서 확인할 조건 |
|---|---|---|
| `Engine.cpp:234,249,646` | D3D 임시 장치 vtable의 Present를 `Hooking::CreateHook`으로 설치한다. ResizeBuffers 콜백은 있으나 설치 호출은 없다. `UnhookD3D`와 `Hooking::Shutdown`은 `MH_ALL_HOOKS`를 비활성화한다. | Present callback 종료 후 WndProc·장치·원본 포인터 해제 순서. ResizeBuffers 동작은 현재 설치되지 않았다는 사실을 기준으로 별도 구현·검증. |
| `Source_impl.inc:354` | user32의 커서·키·메시지 API 6개를 등록한 뒤 전체 hook 활성화. | 입력 캡처 및 다른 훅과의 소유권 분리. |
| `SpeedHack.cpp:304` | QPC, GetTickCount, GetTickCount64, timeGetTime의 대상과 원본 함수를 보관한다. 생성 또는 활성화 실패 시 `RollbackSpeedHooks`에서 각각 비활성화·제거한다. | callback이 사용하는 원본 경로의 수명과 실제 시간 분리. `SpeedHack_Update`는 background loop 외 UI에서도 호출되므로 최초 관리 스레드 설정의 실행 순서를 실측해야 한다. |
| `JewelSettings.cpp:258` | 보조 보주 판정 함수의 기대 바이트를 확인하고 설치한다. 활성화 실패 시 해당 훅만 제거한다. | 사용자 선택 비트와 저장 마스크 보존, 개별 제거 및 callback 종료. |
| `TraitConfigRuntime_impl.inc:487,784,1033,1404` | 이름·설명·효과의 3개 설치 함수와 다섯 대상에서 재사용하는 호환 설치 함수가 있다. | 각 대상의 실패·재시도, 기재 테이블과 반환 문자열 수명, 이름 편집 hook 연결. |
| `TraitTextNameHook.cpp:576` | 런타임 이름 getter 대상의 원본을 보관하고 생성·활성화한다. 활성화 실패 시 제거한다. | 반환하는 편집 문자열의 게시·보관 수명과 동시 callback. |
| `TengiCave.cpp:614` | 실행 코드 검증 후 3개 훅을 묶어 설치한다. 일부 생성·활성화 실패 시 생성한 대상의 비활성화와 제거를 시도한다. | 세 훅과 원본 함수가 공유하는 준비 상태 및 게임 세대. |
| `RulerTransferProposal.cpp:119,141` | 2곳의 원본 바이트를 검증하고 callsite를 별도 thunk로 바꾼다. 끄면 원본 call 바이트를 쓴다. 전역 생성자에서 저장값을 읽어 mode 0 또는 2를 시도한다. | 설정 읽기의 로더 실행 시점, 다른 패치와 충돌할 때 복구, thunk와 실행 중 callback 수명. 저장은 bool이므로 mode 1과 2의 구분은 재시작 후 보존되지 않는다. |
| `Cheats.cpp:493` | `InstallHeroHook` 정의는 있으나 기준 자체 소스에서 호출을 찾지 못했다. callback의 주인공 주소 저장도 주석이다. | 실제 연결된 주인공 기능과 구분하고 외부 호출 여부를 확인한다. |

각 위치는 정적 코드에서 확인한 소유·실패 경로이며 실제 호출 스레드 또는 모든 종료 경로의 증거는 아니다. 직접 코드 패치 및 개별 기능은 [위치 색인](p0/source_locations.csv)을 따라 추가 추적한다.

## 설정과 기능 연결 추가 색인

[추가 설정 키](p0/config_wrapper_keys.csv)는 `Config.cpp`의 정적 문자열 키 36개에 대해 저장·로드 위치를 연결한다. 동적 전기 목록 키와 독립 파일 처리 키 `bRulerTransferProposal`은 이 목록에 포함되지 않는다. [기능 flag 사용 위치](p0/feature_flag_locations.csv)는 기본 설정 62개의 선언·메뉴·실행 사용 후보를 연결한다. 예를 들어 행동력 무한은 `MenuSectionsDomestic.cpp:110`에서 편집하고 `Menu_impl.inc:439`에서 p1 준비 시 `p1 + 0xEE`에 200을 기록한다. 보주 교체 무한은 같은 메뉴의 293행과 `Menu_impl.inc:460`의 `gameBase + 0x5C49` 쓰기로 연결된다. 이 색인은 주석·선언도 포함하므로 실제 조건과 동작은 원문에서 확인해야 한다.

## 주인공 추적 훅 후보의 연결 상태

기준 자체 cpp·h·inc에서 `InstallHeroHook`, `MaximizeHeroStats`, `g_HeroAddr`의 텍스트 참조를 확인했다. `InstallHeroHook`와 `MaximizeHeroStats`는 각각 `Cheats.cpp` 정의와 `Cheats.h` 선언 외 호출을 찾지 못했다. `g_HeroAddr`는 0으로 초기화되며 활성 코드에서 값을 대입하는 위치를 찾지 못했다. `hkHeroLogic`의 R15에서 주소를 얻는 구문은 주석이며, 현재 본문은 원본 함수 포인터만 호출한다. 따라서 이 후보를 현재 주인공 주소를 캡처하는 활성 훅이라고 집계하지 않는다. 주인공 편집의 실제 `p1` 및 `g_savedHeroAddr` 경로와 이름만으로 합치지 않는다.

이 함수가 별도로 호출되는 경우의 구현은 패턴을 찾고 `MH_Initialize() == MH_OK`일 때만 hook을 생성·활성화한다. 생성 성공 뒤 활성화가 실패하면 이 함수에서 생성한 hook을 제거하지 않는다. target 주소는 지역 변수이고 `g_isHeroHookInstalled`만 전역으로 유지하며, 별도 해제 함수는 기준 자체 소스 검색에서 찾지 못했다. 일반 MinHook 전체 종료는 [시작·종료 조사](P0_Lifecycle.md)를 따른다. 외부 모듈의 호출이나 실제 실행 결과는 이 텍스트 대조로 배제하지 않는다.

## 기재 runtime의 개별 MinHook 대상과 조건 패치

`TraitConfigRuntime_impl.inc`의 `EnsureCustomNameHook`, `EnsureCustomDescHook`, `EnsureTraitEffectHook`는 각각 이름 getter, 설명 매핑 getter, 효과 조회 함수에 hook을 설치한다. 별도 `InstallCompatibilityHook`는 `EnsureExtendedTraitCompatibility`에서 **다섯 대상**에 호출된다. 대상 역할은 장수 보유 판정, 대화 효과 연결, 효과 데이터 연결, 월별 효과 보완, 특수 연출 연결이다. 각 대상마다 원본 trampoline·설치 flag·실패 로그 flag가 따로 있다. 따라서 네 설치 함수의 존재를 hook 대상 네 개로 세면 안 된다.

내부 tick은 설정 로드 뒤 이름 hook과 설명 hook 중 하나라도 준비되지 않으면 그 호출의 나머지 처리를 중단한다. 기재 테이블을 찾고 필요한 적용을 마친 뒤 효과 hook 설치에 성공한 경우에만 다섯 호환 hook과 두 사용 조건 바이트 패치를 차례로 유지한다. 호환 함수는 각 호출의 반환값을 묶어 검사하거나 다른 대상 실패 시 앞서 설치한 대상을 일괄 해제하지 않는다. 실패 로그를 한 번만 남기는 flag와 설치 완료 flag는 각각의 재시도 조건과 별개다. 다섯 대상의 실제 callback 스레드와 병렬 호출은 확인되지 않았다.

두 사용 조건 패치는 MinHook이 아니다. 각각 예상 바이트의 세 번째 위치를 제외한 부분이 일치하는지 확인하고, 해당 바이트가 이미 `0x01`이면 활성으로 취급한다. `0x95`이면 `0x01`을 쓰며 그 밖의 값은 적용을 보류한다. 각 tick에서 원본값으로 돌아간 것을 감지하면 재적용한다. 외부 `version.dll` 존재에 따른 내부 runtime 중단과 wrapper의 파일 선택은 [기재 조사](P0_Traits.md)를 따른다.

회귀 조건은 이름·설명 실패로 후속 작업 보류, 효과 hook 실패 후 재시도, 다섯 호환 대상 중 일부만 설치, 각 원본 불일치와 trampoline 실패, 두 조건 바이트가 각각 원본·활성·다른 값인 경우, 외부 DLL 유무와 종료 중 callback이다. 코드 경로의 정적 대조이며 hook을 설치하지 않았다.
