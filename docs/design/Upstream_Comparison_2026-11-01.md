# 원본 대비 구현 상태 조사 — 2026-11-01

## 비교 기준과 조사 범위

| 대상 | 기준 |
|---|---|
| 원본 최신 `main` | [`WOOSEOK99/S8RPKCheats@068ca1d881edf870f7cf06f935cf50e286004af8`](https://github.com/WOOSEOK99/S8RPKCheats/commit/068ca1d881edf870f7cf06f935cf50e286004af8) |
| 포크 조사 시작점 | [`29thnight/S8RPKCheats@2f6acf8fb655477d014406087ed54bc877e761be`](https://github.com/29thnight/S8RPKCheats/commit/2f6acf8fb655477d014406087ed54bc877e761be) |
| 이력 관계 | `git rev-list --left-right --count upstream/main...HEAD` 결과 `0 31`: 원본보다 31개 커밋 앞서고 뒤처진 커밋은 0개 |

원본의 최신 커밋은 포크에 포함되어 있다. 아래 차이는 원본의 후속 커밋을 가져오지 않은 문제가 아니라, 새 구조로 이관한 코드의 기능 차이다. 커밋 수는 위 조사 시작점 기준이다.

원본의 활성 `MainColumn* → MenuSections*`, 무장 상세와 배우자 창, 설정 저장·로드 경로를 현재 UI·명령·데이터 모델과 대조했다. 대분류별 구현 파일 존재와 실제 사용자 조작의 연결 여부를 구분했다.

이 문서는 조사한 소스의 상태를 기록한다. 전체 기능의 실행 동등성, 모든 실패 경로, 게임 버전별 호환성 또는 성능을 입증하지 않는다. `AGENTS.md`에 따라 빌드·테스트·게임 실행은 수행하지 않았다.

## 구조 변화

원본은 `Internal DX11 Base` 아래 메뉴, 전역 상태, 게임 메모리 접근과 hook을 함께 둔다. 포크는 다음 경계로 나누었다.

| 계층 | 현재 역할과 주요 근거 |
|---|---|
| `src/proxy` | `hid`, `dinput8`, `dxgi` 진입점. [공통 진입점](../../src/proxy/entry.cpp)에서 런타임 시작을 요청한다. |
| `src/platform/windows` | 메모리 접근, 코드 패치, hook, 설정 파일 교체, 실제 시각과 게임 배속의 Windows 구현. |
| `src/domain` | 게임 ID, 값 모델, 요청·결과, 편집용 스냅샷. [명령](../../src/domain/commands.h), [무장 상세](../../src/domain/officer_roster.h), [가족 도구](../../src/domain/officer_tools.h). |
| `src/game` | 게임 객체 조회·변경, native adapter, 기능별 작업과 복구 상태. |
| `src/runtime` | 창 메시지 기반 갱신, 명령 반영, 기능 적용 순서, 세션 변경과 종료. [갱신 루프](../../src/runtime/game_loop.cpp), [명령 처리](../../src/runtime/command_commit.cpp), [종료](../../src/runtime/game_stop.cpp). |
| `src/services` | 설정·이름·기재·로그의 파일 처리와 UI 사이 전달. [설정 서비스](../../src/services/config_service_loop.cpp), [UI bridge](../../src/services/ui_bridge.cpp). |
| `src/overlay` | DX11 렌더링, 입력, 스냅샷 표시와 명령 제출. [주메뉴](../../src/overlay/feature_menu.cpp), [오버레이](../../src/overlay/dx11_overlay.cpp). |

구조 분리와 기능 보존은 별도 판단이다. 현재 메뉴에 큰 기능군이 있어도 그 안의 세부 조작까지 모두 이관됐다고 볼 수는 없다.

## 대응 구현을 확인한 주요 영역

아래는 사용자 진입점과 관련 처리 경로가 존재하는 범위다. 실제 게임에서 모든 항목이 정상 동작한다는 의미는 아니다.

| 영역 | 확인한 대응 범위 | 현재 주요 소스 |
|---|---|---|
| 무장 편집 | 기본·외형 수치, 성별·목소리, 재야·부활·도시 이동, 전법·특기·소양·특수능력, 다중 선택 편집 | [주메뉴](../../src/overlay/feature_menu.cpp), [무장 명단](../../src/game/officer_roster.cpp) |
| 일괄 작업 | 전법 35종·특기 24종 일괄 변경, 빈 기재 슬롯의 등급별 랜덤 배정 | [도구 UI](../../src/overlay/officer_tools_panel.cpp), [게임 처리](../../src/game/officer_tools.cpp) |
| 가족·자녀 | 주인공 배우자·자녀 목록, 조기 임관 예약·취소, 임신 상태 표시와 임신 슬롯 배우자 교체 | [가족 처리](../../src/game/family_tools.cpp), [도구 UI](../../src/overlay/officer_tools_panel.cpp) |
| 도시·후방 지원 | 도시 자원·개발 한도 편집, 일괄 최대화, 반란 0, 곡물 환전, 수송 노선, 세력 기술 편집 | [도시 처리](../../src/game/city_management.cpp), [도시 UI](../../src/overlay/city_management_panel.cpp), [노선 저장](../../src/services/city_support_service.cpp) |
| 군단·명품 | 군단 추천·이동·태수·도독 배치, 명품 조회·지급·평정 후 자동 배분 | [군단 처리](../../src/game/corps_deployment.cpp), [명품 처리](../../src/game/specialty_tools.cpp) |
| 내정·교류·평정 | 주요 토글, 보주·도로·도시 전환, 군주 이동 제안, 평정 관련 패치 | [기존 기능 제어](../../src/runtime/legacy_feature_control.cpp), [native 기능 제어](../../src/runtime/native_feature_control.cpp) |
| 전투 | 날씨·일자·경과일 스킵·지형·공성·치유, 전법·건물 설정, 포로·원군 정책 | [주메뉴](../../src/overlay/feature_menu.cpp), [명령 처리](../../src/runtime/command_commit.cpp), [전투 갱신](../../src/runtime/battle_dispatch.cpp) |
| 성장·기재 | AI 성장·특수능력 자동 배정, 기재 3슬롯·이름·설명, 사용자 기재 설정 읽기 | [AI 성장](../../src/game/ai_officer_growth.cpp), [특수능력 배정](../../src/game/special_ability_assignment.cpp), [기재 제어](../../src/runtime/trait_control.cpp) |
| 편의·설정 | 배속, UI 배율, 시작 시 메뉴 표시, 위젯·알림, 파일 로그, 설정 저장 | [배속](../../src/platform/windows/speed_clock.cpp), [설정 서비스](../../src/services/config_service_loop.cpp), [파일 로그](../../src/services/file_log.cpp) |
| 5번 책략 | native 버튼 생성·등록·callback, index 4 접근, 데이터·개수·진영·모델 연결 | [native adapter](../../src/game/stratagem5_native.cpp), [전투 갱신](../../src/runtime/battle_dispatch.cpp) |

원본 `ConfigBase.inc`와 `Config.cpp`의 저장·로드 키 문자열을 현재 `src`와 대조한 범위에서는 기능 키 누락을 발견하지 못했다. 키가 존재하는 것만으로 기능 동등성을 판단하지 않는다. 다음 세부 조작은 설정 키 중심 조사로 확인하기 어렵다.

## 확인된 세부 기능 미이관

### 1. 소속 세력 색상 편집

원본 무장 상세는 무장 객체 `+0x18`의 세력 포인터를 읽고, 세력 객체 `+0x09` 한 바이트를 편집하는 `세력 색상` 행을 제공한다. 주인공 상세도 같은 헤더 표시 함수를 사용한다.

- 원본: [SelectOfficercapture_impl.inc의 세력 색상 행](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc#L199-L231).
- 현재: [OfficerDetail](../../src/domain/officer_roster.h)에는 세력 색상 필드가 없고, [편집 명령](../../src/domain/commands.h)과 [무장 상세 UI](../../src/overlay/feature_menu.cpp)에도 해당 조작이 없다.

따라서 기본 능력·외형 편집이 존재하더라도 세력 색상까지 이관된 상태는 아니다. 추가할 때는 무장 ID뿐 아니라 세력 객체의 정체성과 변경 전 색상도 확인해야 한다.

### 2. 선택 무장의 관계 정보 표

원본 공통 무장 상세는 의형제·배우자·상생·상극을 표시하고, 관계가 발견되면 원수·호적수도 표시한다.

- 원본: [상세에서 관계 표 호출](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc#L921-L924), [관계 표 구현](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc#L1725-L1850).
- 현재: [OfficerDetail](../../src/domain/officer_roster.h)의 `family[3]`과 [무장 상세 UI](../../src/overlay/feature_menu.cpp)는 가문·부·모를 제공한다. 일반 선택 무장의 관계 목록은 스냅샷에 없다.
- [가족 도구 UI](../../src/overlay/officer_tools_panel.cpp)는 주인공의 배우자·자녀 목록이다. [군단 추천 관계 수집](../../src/game/corps_relationships.cpp)도 상세창의 관계 표시와 연결된 기능은 아니다.

가족 기능 전체의 부재가 아니라 일반 무장 상세의 관계 조회가 빠진 상태다. 관계 수집 결과를 ID·epoch에 묶인 조회 스냅샷으로 제공하는 후속 구현이 필요하다.

### 3. 배우자 관계 목록의 두 항목 순서 교환

원본 배우자 창은 두 행을 선택한 뒤 관계 테이블의 두 포인터 값을 맞바꾸는 조작을 제공한다.

- 원본: [활성 배우자 창의 두 항목 교환](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_t05_base.inc#L552-L599).
- 활성 경로: [SelectOfficercapture.cpp](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture.cpp#L1-L3)가 위 wrapper를 포함한다. wrapper가 기존 배우자 창을 legacy 이름으로 분리하고 현재 진입점을 제공하므로 폐기된 스캐너의 조작을 세어 넣은 것이 아니다.
- 현재: [OfficerToolAction](../../src/domain/officer_tools.h)에는 관계 테이블 두 항목의 교환 명령이 없다. [임신 슬롯 UI](../../src/overlay/officer_tools_panel.cpp)의 `SetPregnancySpouse`는 비임신 슬롯 하나에 아직 등록되지 않은 배우자를 넣는 다른 작업이다.

이관하려면 관계 테이블·두 대상의 정체성을 검증하고 두 쓰기 중 하나가 실패했을 때 복구할 명령이 필요하다. 원본의 직접 포인터 교환 코드를 UI에 그대로 옮기는 방식은 현재 구조와 맞지 않는다.

### 부속 조작: 배우자 이름에서 상세창 열기

[원본 배우자 이름](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_t05_base.inc#L503-L514)은 `Selectable`이며 클릭하면 해당 무장의 독립 상세창을 연다. 현재 [가족 도구 UI](../../src/overlay/officer_tools_panel.cpp)의 `draw_spouses()`는 이름을 `Text`로 표시한다. 자녀 보기와 임신 슬롯 대상 선택은 있지만 상세창 바로가기는 없다.

## 구현 차이와 미구현을 구분할 항목

| 항목 | 판단 |
|---|---|
| 작업 분할·명령 전달 | 현재는 긴 작업을 여러 갱신에 나누고 진행·취소 상태를 전달한다. 원본과 호출 시점이 다르다는 이유만으로 기능 부재로 분류하지 않는다. |
| UI 배율 | 현재 `fUiScale`은 폰트와 레이아웃을 함께 조정한다. 원본의 폰트 배율과 의도적으로 다르다. |
| 게임 버전 확인 | 5번 책략과 일부 native getter는 지원 실행 파일 식별자를 확인한다. 다른 빌드의 거부는 무조건 적용하던 동작의 누락으로 보지 않는다. |
| 독립 기재 편집기 | 원본 `tools/S8RPK_Trait_INI_Editor.py` 및 EXE 빌드 스크립트는 현재 최상위 작업 공간에 없다. [인게임 편집기](../../src/overlay/trait_text_editor.cpp)가 기본·확장 이름/설명 적용·저장을 제공하므로 편집 기능 전체의 부재와 외부 도구 배포 형태의 차이를 구분한다. |
| 자동 무장 캡처 hook | 원본 `SelectOfficercapture_impl.inc`에서도 관련 hook은 이미 legacy로 분리되어 비활성이다. 포크의 신규 미이관으로 집계하지 않는다. |
| 능력 EXP 편집 | 원본의 현재/누적 EXP는 진단용 읽기 전용 표시다. 원본에 편집 기능이 있었다고 전제해 미구현 목록을 만들지 않는다. |
| `bBattleUnit` | 원본의 관측용 캡처를 현재 전투 관측 경로로 대체한 항목이다. 과거 설정 키와 같은 이름의 setter가 없다는 사실만으로 사용자 기능의 누락을 단정하지 않는다. |

## 구현은 있으나 남은 수명 계약·실행 확인

### 5번 책략 버튼의 파괴·해제

[native adapter 계약](../../src/game/stratagem5_native.h)은 확인되지 않은 게임 destructor/free ABI를 추정해 호출하지 않는다. [구현](../../src/game/stratagem5_native.cpp)은 소유권 이전이 확인되지 않은 할당을 제한된 기록에 남기고, 64개 한도에 도달하면 새 확장을 보류한다.

버튼 생성·등록·callback은 구현되어 있다. 게임 소유 객체의 파괴 계약, 부분 생성 실패 후 회수와 dialog 재사용의 실제 동작은 별도 확인이 남아 있다. 이 상태를 `5번 책략 전체 미구현`이라고 부르면 부정확하다.

### 실행 확인

- Windows에서 각 프록시의 컴파일·링크·로딩과 설정 서비스 시작.
- 저장 불러오기, 주인공·시나리오 전환, 가족·도시·군단 작업, 전투 종료에 따른 대상 수명과 복구.
- native UI callback의 실제 실행 스레드와 창 소유 스레드 일치 여부.
- ResizeBuffers, 장치·창 소실, 종료 복구 실패와 다음 갱신의 재시도.
- 실제 frame 시간, CPU/GPU 사용량, 60FPS 유지 여부.

소스의 작업량 제한은 위 성능 수치를 측정한 결과가 아니다. 이번 작업에서는 빌드·테스트·게임 실행이나 성능 측정을 수행하지 않았다.

## 문서 상태와 후속 순서

조사 시작점의 [README](../../README.md)는 게임 읽기·쓰기와 설정·UI·hook을 아직 연결하지 않았다는 초기 설명을 담고 있어 현재 구현보다 오래됐다. 이번 변경에서 현재 구조와 잔여 항목 안내로 수정했다. [기능 대응 문서](Implementation_Parity.md)도 대분류 연결이 모든 세부 조작의 이관 완료를 뜻하지 않도록 수정했다.

메뉴가 잠기는 문제의 수정과 별도로, 세부 기능 보존 작업은 관계 조회·상세 바로가기, 세력 색상 편집, 배우자 관계 순서 교환 순서로 진행할 수 있다. 마지막 항목은 조회 UI보다 쓰기·실패 복구 계약의 검토 범위가 크다.

이 조사는 확인된 누락과 대응 경로를 기록한 것이다. 목록에 없는 모든 세부 기능의 동등성이 증명되었다고 해석하지 않는다.

## 설정 로딩 표시와 편집 잠김 수정

### 증상과 코드상 조건

첨부 화면은 게임 진행 중에도 `설정을 불러오는 중입니다.`와 비활성 컨트롤을 표시한다. [메뉴](../../src/overlay/feature_menu.cpp)의 준비 상태는 게임 화면이 아니라 `UiSnapshot.config.config_loaded`에 의해 결정된다. 설정 파일을 읽은 뒤에도 창 소유 스레드가 이를 소비하고 스냅샷을 게시하지 않으면 편집이 열리지 않는다.

수정 전에는 최초 창 연결만 `WH_GETMESSAGE`로 처리한 뒤 hook을 해제했다. 이후 갱신은 `PostMessageW → DispatchMessage → subclass` 경로에 의존했다. 게임이 등록 메시지를 큐에서 제거하고 창 프로시저로 보내지 않으면 `update_pending`이 해제되지 않으며 다음 Present도 갱신을 보낼 수 없다. 오버레이 렌더링은 별도 경로여서 계속 보일 수 있다.

이 경로가 동일 증상을 만든다는 것은 코드로 확인했지만, 사용자의 실행 중 메시지 루프와 설정 파일을 직접 관측하지는 않았다. 따라서 스크린샷의 유일한 원인으로 단정하지 않는다.

### 반영한 변경

| 변경 | 결과 |
|---|---|
| [갱신 메시지 처리](../../src/runtime/game_loop.cpp) | 성공적으로 연결한 창의 `WH_GETMESSAGE`를 세션 동안 유지한다. 자체 갱신 메시지만 `PM_REMOVE`에서 같은 창 소유 스레드로 전달하고 `WM_NULL`로 바꾸어 중복 처리를 막는다. `PM_NOREMOVE`와 일반 입력은 처리 대상이 아니다. |
| 종료 시 hook 정리 | coordinator가 hook을 해제하고 실행 중 갱신 및 관련 hook callback이 끝난 뒤 서비스를 정리한다. 해제 실패 시 코드·소유자·서비스를 보존한다. |
| [최초 설정 전달](../../src/runtime/command_commit.cpp) | 게임 상태 관측과 순환 작업 전에 설정 수신·로컬 값 적용·baseline·최초 스냅샷 게시를 수행한다. 관측이 예산을 소모해도 첫 설정의 게시가 뒤로 밀리지 않는다. |
| [설정 적용의 게임 접근 분리](../../src/runtime/runtime.cpp) | 초기 5번 책략 설정은 정규화된 로컬 값으로 보관한다. 게임 테이블 반영은 기존 책략 lifecycle이 담당한다. |
| [UTF-8 BOM 호환](../../src/services/config_json.cpp) | JSON 라이브러리는 허용하지만 자체 키 scanner가 거부하던 BOM을 건너뛴다. 원본 BOM·알 수 없는 키·값·서식은 저장할 때 보존한다. |
| [로드 오류 게시](../../src/services/config_service_loop.cpp), [화면 표시](../../src/overlay/feature_menu.cpp) | JSON/중복 키 오류와 파일·서비스 오류를 로딩 문구 대신 표시한다. 파일을 고쳐 재시도에 성공하면 초기 오류 상태를 지운다. 잘못된 파일을 기본값으로 덮어쓰지 않는다. |

Win32 처리 근거: [GetMsgProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/getmsgproc)는 메시지가 게임 코드에 반환되기 전에 호출되며 `MSG` 수정을 허용한다. [UnhookWindowsHookEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-unhookwindowshookex)는 반환 뒤에도 실행 중 callback이 남을 수 있어 별도 종료 확인이 필요하다.

### 검토 범위와 재컴파일 영향

- `git diff --check`와 설정 수신·게시, 메시지 중복/재진입, hook 종료 순서의 소스 검토를 수행했다.
- 기존 [회귀 fixture](../../tests/stratagem5_tests.cpp)에 BOM 설정 로드, 저장 시 접두부·기존 내용 보존, 중복 키 거부 사례를 추가했다. 실행하지 않았다.
- 빌드·테스트·게임 실행은 `AGENTS.md` 지침에 따라 수행하지 않았다. 사용자 환경의 실제 편집 복구와 게임 빌드 호환성은 실행 확인이 남아 있다.
- 선언을 추가한 `command_commit.h`는 `command_commit.cpp`, `game_loop.cpp`, 테스트 번역 단위가 직접 포함한다. 나머지는 변경된 Runtime·Services·Overlay의 `.cpp`와 링크하는 프록시가 영향 범위다. 빌드 설정·외부 의존성은 변경하지 않았다.
