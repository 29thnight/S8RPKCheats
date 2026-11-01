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

## 확인된 세부 기능의 후속 이관

초기 조사에서 아래 네 조작의 누락을 확인했다. 설정 로딩 수정 커밋 [`e3682a3`](https://github.com/29thnight/S8RPKCheats/commit/e3682a3528081d8ad8dd89a7d5ec642f39113aee) 이후, 사용자의 후속 구현 요청에 따라 현재 소스에 연결했다. 아래의 구현 완료는 Windows 빌드나 인게임 동작 확인을 뜻하지 않는다.

### 1. 소속 세력 색상 편집

원본 무장 상세는 무장 객체 `+0x18`의 세력 포인터를 읽고, 세력 객체 `+0x09` 한 바이트를 편집하는 `세력 색상` 행을 제공한다. 주인공 상세도 같은 헤더 표시 함수를 사용한다.

- 원본: [SelectOfficercapture_impl.inc의 세력 색상 행](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc#L199-L231).
- 후속 구현: [값 모델](../../src/domain/officer_roster.h), `SetOfficerFactionColor` [명령](../../src/domain/commands.h), [조회·쓰기](../../src/game/officer_roster.cpp), [무장 상세 UI](../../src/overlay/feature_menu.cpp)를 연결했다.

주인공 상세와 모든 무장 편집의 선택 상세에서 0~255 색상 번호를 입력하고 적용한다. 소속 세력 전체의 색상 값이다. UI에는 게임 주소 대신 무장 ID·검증 token·관측한 색상만 전달한다. 쓰기 직전에 세션·무장·세력 정체성과 기존 값을 다시 확인하고 readback한다. 실패한 복구 기록은 캐시에 보존하며 다른 상세창을 통한 색상 쓰기도 차단한다. 창을 닫아도 복구를 재시도하고 종료 단계에서도 복구 완료를 확인한다.

### 2. 선택 무장의 관계 정보 표

원본 공통 무장 상세는 의형제·배우자·상생·상극을 표시하고, 관계가 발견되면 원수·호적수도 표시한다.

- 원본: [상세에서 관계 표 호출](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc#L921-L924), [관계 표 구현](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc#L1725-L1850).
- 후속 구현: 주인공·선택 무장 각각의 `OfficerRelations` 스냅샷을 [조회 캐시](../../src/game/officer_roster.cpp)에서 수집하고 [상세 UI](../../src/overlay/feature_menu.cpp)에 표시한다. 가문·부·모 표시와 주인공 가족 도구도 유지한다.

의형제·배우자·상생·상극·원수·호적수의 이름·ID와 읽을 수 있는 친밀도를 제공한다. 조회 결과는 ID·epoch에 묶고, 게임 경계의 작업량·시간 예산 안에서 여러 번에 나눠 수집한다. Present는 완료된 값 스냅샷만 읽는다. `없음`, `조회 불가`, `일부 조회`, `읽기 실패`를 구분하고, 게임 내 관계가 바뀌면 `관계 갱신`으로 다시 읽는다. 원본의 알려진 후보 위치를 검증하며, 불확실한 메모리를 넓게 검색해 임의의 표를 채택하지 않는다.

### 3. 배우자 관계 목록의 두 항목 순서 교환

원본 배우자 창은 두 행을 선택한 뒤 관계 테이블의 두 포인터 값을 맞바꾸는 조작을 제공한다.

- 원본: [활성 배우자 창의 두 항목 교환](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_t05_base.inc#L552-L599).
- 활성 경로: [SelectOfficercapture.cpp](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture.cpp#L1-L3)가 위 wrapper를 포함한다. wrapper가 기존 배우자 창을 legacy 이름으로 분리하고 현재 진입점을 제공하므로 폐기된 스캐너의 조작을 세어 넣은 것이 아니다.
- 후속 구현: [OfficerToolAction](../../src/domain/officer_tools.h)에 `SwapSpouseOrder`를 추가하고 [가족 처리](../../src/game/family_tools.cpp)와 [배우자 UI](../../src/overlay/officer_tools_panel.cpp)에 연결했다. `SetPregnancySpouse`와는 별도 명령·선택 상태다.

가족 도구의 배우자 목록은 관계 레코드·member 순서로 표시한다. 두 행의 `교환`을 체크한 뒤 `선택한 배우자 순서 교환`을 누른다. ID·epoch·관계 generation·관측 순서와 실제 두 포인터를 다시 확인한 뒤 태그 비트를 포함한 값을 교환한다. 동일 배우자가 여러 곳에 중복 참조된 경우 교환을 허용하지 않는다. 두 쓰기 중 실패가 발생하면 원래 값을 복구하고, 복구하지 못한 기록은 refresh·세션 변경·종료에서 지우지 않는다. 다른 세대의 객체에는 복구 값을 쓰지 않으며 미복구 기록이 있으면 새 교환과 종료 완료를 보류한다.

원본 활성 스캐너는 `record + 0x10`을 `pointerAddr`로 보관하면서 실제 배우자를 `record + 0x18`에서 읽는다. 원본 버튼이 이 `pointerAddr`에 배우자를 쓰는 동작은 주인공 포인터를 덮을 수 있다. 이번 구현은 선택 배우자의 실제 member 위치를 기록해 그 두 필드만 변경한다.

### 부속 조작: 배우자 이름에서 상세창 열기

[원본 배우자 이름](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_t05_base.inc#L503-L514)은 `Selectable`이며 클릭하면 해당 무장의 독립 상세창을 연다. 후속 [가족 도구 UI](../../src/overlay/officer_tools_panel.cpp)도 이름 클릭을 지원하고, 현재 구조에서는 기존 `모든 무장 편집 리스트`를 펼쳐 해당 무장의 상세로 이동한다.

목록이 없으면 한 번 조회를 요청하고 완료 후 정확한 ID로 선택한다. 기존 신분 필터와 다중 편집 선택을 해제해 대상이 가려지거나 다른 무장까지 편집되는 것을 막는다. 큐 접수 실패 시 이동 요청을 유지하며 자동 무한 재조회는 하지 않는다. 새 관계 표의 무장 이름에서도 같은 이동 경로를 사용한다.

### 이관 변경의 검토와 재컴파일 범위

- `git diff`와 `git diff --check`, 별도 검토자의 요청·상태·포인터 필드·실패 복구 경로 검토를 수행했다. 빌드·테스트·게임 실행은 수행하지 않았다.
- 새 명령·불변 스냅샷·캐시를 연결하므로 `domain/commands.h`, `domain/config_snapshot.h`, 무장·가족 모델 및 Runtime·UiBridge·패널 헤더가 변경됐다. 이를 포함하는 Game·Runtime·Services·Overlay 번역 단위와 종속 프록시가 재컴파일·링크 대상이다. 빌드 설정과 의존 프로젝트는 변경하지 않았다.
- 실행 환경에서는 무소속/세력 변경, 주인공 전환, 빈 관계/조회 실패, 배우자 두 행 교환 후 새로고침·재로드, 오래된 화면의 요청 거부와 복구 실패 시 종료 보류를 확인해야 한다. 이는 실행한 테스트 목록이 아니다.

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

메뉴가 잠기는 문제 수정에 이어 위 네 세부 조작을 이관했다. 후속으로 남은 범위는 Windows 빌드·게임 실행 확인과 위에 구분한 native 수명 계약이다. 이 문서에 없는 모든 세부 조작의 기능 동등성을 완료로 단정하지 않는다.

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

## 설정 로드 이후에도 정보가 비고 편집이 잠기는 문제

사용자는 후속으로 설정은 읽었지만 게임 정보가 없고 편집이 비활성이라는 증상을 보고했다. 실행 중인 사용자 프로세스를 직접 관측하지는 못했으므로, 아래는 해당 증상을 만들 수 있는 확정된 **코드 경로**와 그 수정이다.

### 1. 초기 주소 검색 실패의 영구 고정

기존 [coordinator](../../src/runtime/game_loop.cpp)는 시작할 때 `initialize_game_base_from_module()`을 한 번 호출하고 실패를 무시했다. [관측 함수](../../src/runtime/runtime.cpp)는 `game_base_pointer == 0`이면 매번 반환했으며, 재탐색 경로가 없었다. 초기 image vector도 같은 module이면 재사용하므로 함수만 다시 호출해도 준비 전 snapshot의 검색 실패가 반복된다.

[게임 주소 탐색](../../src/game/game_base.cpp)에 실제 실행 메모리의 점진 탐색을 추가했다. 4KiB 블록과 9바이트 경계 정보를 사용하고 호출당 최대 64KiB 및 deadline으로 작업을 나눈다. 최초 구현은 RIP 상대 슬롯의 모듈 범위·정렬·메모리 속성, root/P1 접근과 무장 ID를 검증했다. 후보 명령도 현재 메모리에서 다시 읽어 오래된 overlap과 새 bytes를 잘못 결합하지 않는다. 이때 P1을 필수로 요구해 타이틀 편집을 막는 회귀가 생겼으며, 아래 후속 수정에서 기본 기재 테이블을 통한 독립 검증과 기능별 준비 조건을 추가했다.

처음 루트를 복구하면 coordinator가 별도 vector로 실행 image를 새로 읽고 signature·metadata 값을 준비한다. 완료 packet은 창 소유 스레드에서만 채택한다. 초기 실패로 남은 기능 주소는 누락 필드만 합치며 활성 hook 주소·원본·cave·복구 기록을 덮지 않는다. 미적용 native 실패, 게시 전 책략 준비 실패와 이름 getter 실패도 해당 경계에서 다시 확인한다.

### 2. 선행 조회가 모든 후속 작업을 굶기는 경로

기존 갱신은 1,500µs deadline을 만든 뒤 필수 root 관측과 반복 쓰기를 먼저 수행했다. 이 구간이 매번 예산을 소모하면 순환 job의 위치도 전진하지 않는다. 설정 bootstrap은 앞선 수정으로 보이지만, 이후 무장 정보 샘플·명령 완료·snapshot 게시가 계속 실행되지 않을 수 있다. 이 상태에서 접수된 편집 요청은 UI의 `pending`을 풀지 못한다.

필수 root/epoch 관측 후 순환 작업의 예산을 시작하고, 반복 쓰기는 별도 `Recurring` job으로 옮겼다. 탐색은 게임 상태를 아직 읽지 못해도 실행되는 `Discovery` job이다. 긴 작업은 기존처럼 다음 순환 위치를 먼저 기록하므로 이후 메시지에서 다른 작업이 진행한다. 이 변경은 전체 frame이 1,500µs 이내라는 보장이 아니다.

### 화면에서 확인할 상태

[메뉴](../../src/overlay/feature_menu.cpp)에 설정 준비와 별도의 게임 메모리 상태를 추가했다.

| 표시 | 의미 |
|---|---|
| 게임 메모리 주소를 찾는 중 · 진행률 | 실제 실행 메모리 탐색이 진행 중이다. |
| 게임 준비 또는 주소 재검색 대기 · 조회 횟수 | 해당 sweep에서 준비된 root/P1을 확인하지 못했으며 자동 재시도한다. 이 상태만으로 게임 버전 비호환을 단정하지 않는다. |
| 실행 파일 메모리 / 게임 기본 정보 / 주인공 읽기 실패 | 실패한 단계가 구분된다. 읽기 실패는 세션이 끝났다는 증거로 사용하지 않는다. |
| 게임 정보 연결됨 · 주인공 ID | root/P1 관측과 주인공 snapshot 전달이 진행됐다. 개별 native 기능의 호환성까지 보장하는 표시는 아니다. |

관련 변경도 `git diff`와 정적 교차 검토 범위다. Windows 빌드·게임 실행으로 사용자의 실제 증상 해소를 확인하지는 않았다.

## 후속 확인: 메인화면 편집과 주인공 의존성

추가 비교 기준은 위 원본 `068ca1d`와 포크 [`e3a1a59`](https://github.com/29thnight/S8RPKCheats/commit/e3a1a597c68efd30fc401d0f014005e7ca085cea)이다. 사용자가 보고한 메인화면 편집 제한은 원본과 다른 조건으로 확인했다.

### 원본이 정보를 가져오는 조건

원본 [무장 메뉴](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/MenuSectionsOfficerEdit.cpp)는 주인공 버튼에만 P1 조건을 둔다. [모든 무장 창](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/SelectOfficercapture_impl.inc)은 P1과 별도로 정적 무장 배열을 찾고, 둘 다 없는 경우에만 진입 안내를 표시한다. [정적 배열 resolver](https://github.com/WOOSEOK99/S8RPKCheats/blob/068ca1d881edf870f7cf06f935cf50e286004af8/Internal%20DX11%20Base/Cheats/Officer/OfficerRosterResolve.cpp)는 EXE의 `0x034c8630` 슬롯과 `{0x48, 8, 0x10, 0, 8}` 체인을 사용하며 P1 인자가 없다.

| 작업 | 필요한 데이터 | 후속 수정의 처리 |
|---|---|---|
| 모든 무장 목록·선택·기본 수치·성별·음성·전법·특기·소양 | 모듈의 정적 무장 배열과 현재 대상 ID | 루트/P1 발견 전에도 요청과 스캔을 진행한다. 각 조회·쓰기의 배열/대상 검증을 유지한다. |
| 무장에게 기재 배정 | 대상 무장, 검증된 root와 기재 객체 | 일반 무장에는 P1이 필요 없다. root/기재 데이터가 없으면 대기·실패 조건을 유지한다. |
| 기본 기재 이름·설명, 특수능력 개수 설정 | 파일·편집 데이터와 검증된 모듈 훅 | P1 없는 상태에서도 편집 요청과 기재 제어 작업을 처리한다. |
| 확장 기재 효과 | 카탈로그, root, 실제 효과 테이블 | 파일 로드와 테이블 적용을 구분하고 테이블이 없으면 `WaitingForTable`을 유지한다. 외부 `version.dll` 소유 상태도 유지한다. |
| 도시 기본 조회·필드 편집·최대화·수동 반란 초기화 | 독립 정적 도시 배열 | P1 조건을 제거하고 쓰기 직전에 도시 배열 정체성을 재확인한다. |
| 주인공 자원·가족, 도시 수송·세력 기술·자동 유지, 전투 | 해당 시나리오·주인공·세력·전투 데이터 | 관련 준비 조건을 유지한다. |

원본도 정적 배열이나 필요한 게임 객체 자체가 없으면 편집할 수 없다. 이 비교는 메인화면에서의 **허용 조건**을 확인한 것이며, 사용자의 실행 파일이 모든 테이블을 언제 준비하는지 측정한 결과는 아니다.

### 회귀 원인과 반영한 수정

`e3a1a59`의 `validate_game_base_pointer()`는 P1과 유효한 무장 ID가 있어야 루트를 확정했다. 이어 `loop_context_observed` 공통 조건이 무장 스캔, 편집 명령, 기재 제어까지 막았다. 실제 무장 resolver와 기재 문구 처리는 그 조건이 필요하지 않았다.

- [루트 탐색](../../src/game/game_base.cpp): 정상적으로 읽은 P1 값이 0이면 기본 기재 1·2·70의 포인터·연속 레코드·ID·공통 모듈 vtable을 두 번 확인하는 대체 경로를 사용한다. null root, P1 읽기 실패, 유효하지 않은 nonzero P1을 승인하지 않는다.
- [명령과 스케줄](../../src/runtime/command_commit.cpp): 독립적으로 처리할 수 있는 명령을 명시하고 무장 스캔을 UI의 목록 발행 작업으로 이동했다. 루트를 아직 찾지 못해도 기재 문구·모듈 훅을 준비한다. 이미 알던 루트의 읽기 실패나 발견 직후 첫 관찰 전에는 무장 쓰기를 보류하고 기존 기재 적용·복구 기록을 유지한다. 스캔 중 정적 무장 배열이 바뀌어도 새 결과를 발행하기 전에 epoch를 갱신한다.
- [세션 전환](../../src/runtime/runtime.cpp): P1이 없는 정상 루트도 기록한다. P1의 `0 → 실제 포인터` 변화, 루트 변경, 루트 발견 전 목록 사용 후 최초 연결에서 이전 명령의 epoch를 폐기한다. 열려 있던 목록은 자동 재조회한다. 읽기 실패만으로 세션을 종료하지 않는다.
- [도시 처리](../../src/game/city_management.cpp), [도시 서비스](../../src/runtime/city_control.cpp): 기본 도시 표와 시나리오 작업을 분리했다. 수송·세력 기술은 현재 P1을 검증하고, 자동 환전·연간 수송·자동 반란 유지는 시나리오에서만 실행한다. 루트 읽기 실패 시 기존 복구 기록을 보존한다.

### 첨부 화면의 상태와 UI 수정

스크린샷의 `주인공 ID 565`는 현재 P1 조회 결과다. `요청 실패 (5, 21)`의 5는 기능 번호가 아니라 `PlatformFailure`, 21은 준비 전 요청 가드의 `ERROR_NOT_READY`다. 이전 요청 오류는 이후 연결만으로 지워지지 않았으므로 두 표시가 함께 남을 수 있다. 숫자만으로 실패한 요청이나 현재 모든 기능의 성공 여부를 특정할 수 없다.

[메뉴](../../src/overlay/feature_menu.cpp)는 이제 설정 불러오기, 시나리오 연결, 공용 무장 목록 조회를 구분한다. 오류는 한국어 이유와 `이전 요청 실패`로 표시하고 원래 코드는 tooltip으로 제공한다. 체크가 꺼진 항목에 공유 패치의 `적용됨`을 그대로 표시하지 않는다. 체크는 사용 설정이며 각 기능의 실제 적용 성공을 일괄 증명하지 않는다.

이번 수정에 새 스프라이트 테마나 대분류 페이지 재구성은 포함하지 않았다. 현재 PNG 리소스 → WIC 디코드 → DX11 텍스처 → ImGui 이미지 버튼 경로를 기반으로, 제공받은 버튼 상태별 이미지·atlas·패널을 추가하고 선택 분류만 표시하는 UI를 구현할 수 있다. 상태별 UV와 9-slice 프레임은 추가 구현이 필요하다. [참고 모드](https://steamcommunity.com/sharedfiles/filedetails/?id=3717461054)의 공개 화면에서 확인한 분류 탐색과 선택 표시를 참고하되, 실제 동작 전체가 동일하다고 단정하지 않는다.

검토는 변경 diff와 독립적인 소스 교차 검토로 한정했다. `AGENTS.md`에 따라 빌드·테스트·게임 실행은 수행하지 않았다.
