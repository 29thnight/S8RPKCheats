# 원본 기능 대응과 구현 상태

## 비교 기준과 확인 범위

- 원본: `WOOSEOK99/S8RPKCheats`, `068ca1d881edf870f7cf06f935cf50e286004af8`.
- 리팩토링 출발점: `29thnight/S8RPKCheats`, `53b8cba15e408f46900443ba2e272c39dee0944f`.
- 구현 기준: 원본의 실제 `MainColumn* → MenuSections*` 호출과 게임 기능의 사용 경로. 원본의 진단용 분기와 사용되지 않는 실험 코드를 기능 완료의 근거로 세지 않는다.
- 이 문서는 **소스 구현·연결 상태**를 기록한다. `AGENTS.md`에 따라 빌드, 테스트, 게임 실행, FPS 측정은 하지 않았다. 실행 시 기능의 일대일 동작과 60FPS 유지는 아직 입증하지 않았다.

원본의 메뉴 배치를 복제하지는 않았다. 동일한 조작은 현재 메뉴·편집 패널·명령을 거쳐 실행되며, 변경 전 값과 대상의 수명을 확인하는 절차를 추가했다.

## 기능 대응

아래 경로는 이 저장소 루트 기준이다. 기존에 구현된 기본 기능은 유지하고, 누락된 사용자 진입점과 실행·저장·종료 경로를 연결했다.

| 원본 기능 | 현재 주요 구현 | 구현 및 개선 |
|---|---|---|
| 실행 진입점, 입력, 메뉴, 종료 | `src/runtime/game_loop.cpp`, `src/overlay/window_subclass.cpp`, `src/runtime/game_stop.cpp` | loader lock 밖의 초기화, 창 소유 스레드의 메시지 처리, 중복 실행 방지, 단계별 종료·복구. Present는 갱신 메시지를 합쳐 보낸다. |
| DX11 오버레이·크기 변경 | `src/overlay/dx11_overlay.cpp`, `src/overlay/overlay_present.cpp` | RTV를 보관하고 크기 변경·장치/스왑체인 변경 때 무효화한다. 매 Present마다 RTV를 생성하지 않는다. |
| 설정과 로그 | `src/services/config_service_loop.cpp`, `src/services/config_changes.cpp`, `src/services/file_log.cpp` | 파일 처리는 서비스에서 수행한다. 요청·완료 알림과 deadline으로 깨우며 고정 주기 sleep을 사용하지 않는다. 로그 생산자는 짧은 try-lock과 유한 큐를 사용한다. |
| 게임 배속 | `src/platform/windows/speed_clock.cpp`, `src/runtime/legacy_feature_control.cpp` | 1.0–5.0배, 0.5배 간격. 플러그인 작업에는 실제 시각을 사용하고 게임 시각의 연속성을 유지한다. 설치 요청은 다른 패치와 순환 처리한다. |
| 시나리오·전기 | `src/game/start_setting.cpp`, `src/game/tengi.cpp`, `src/game/officer_tools.cpp` | 초기 시나리오 보정, 연월 편집, 데모 중지, 15개 전기의 허용·기간·간격 설정. 데모 중지는 실제 관측한 P1 객체의 정체성을 다시 확인한다. |
| 주인공·전체 무장 편집 | `src/game/officer_roster.cpp`, `src/game/officer_tools.cpp`, `src/overlay/officer_tools_panel.cpp` | 기존 기본 능력·적성·소속·기재 편집에 전법 35종·특기 24종 일괄 변경, 대상 필터, 랜덤 기재 배정·중복 방지·빈 칸 채우기를 연결했다. |
| 가족·자녀 | `src/game/family_tools.cpp`, `src/game/native_officer_names.cpp` | 조기 임관 예약/취소, 임신·배우자 편집, 실제 게임 이름/자 조회. 이름 파일의 비어 있지 않은 사용자 지정 값이 우선한다. |
| AI 성장·특수능력 | `src/game/ai_officer_growth.cpp`, `src/game/special_ability_assignment.cpp` | 연도·세션 단위 중복 방지, 단계별 읽기·반영, 수동 실행·자동 실행과 결과 표시. |
| 도시 관리 | `src/game/city_management.cpp`, `src/overlay/city_management_panel.cpp` | 51개 도시의 자원·개발 한도·반란, 일괄 변경, 곡물 환전과 자동 유지. 세력·테이블·변경 전 값을 재확인한다. |
| 후방 지원 | `src/runtime/city_control.cpp`, `src/services/city_support_service.cpp` | 수동 수송, 정액/비율 노선, 연말→연초 자동 실행. 원본 `S8RPK_rear_support_routes.json` 형식을 읽고 서비스에서 저장·재읽기한다. |
| 세력 기술 | `src/game/city_management.cpp`, `src/overlay/city_management_panel.cpp` | 120개 세력 슬롯의 5종 기술 수동 편집. 세력·군주를 확인하고 실패한 여러 필드 변경을 복구한다. 기존 기술 초기화 기능과 별개로 제공한다. |
| 군단 추천·배치 | `src/game/corps_deployment.cpp`, `src/game/corps_plan.cpp`, `src/game/corps_relationships.cpp`, `src/game/corps_mutation.cpp` | 의형제→상생→배우자 결합 순서, 추천·일반 이동·태수 최초 임명·태수 순환·도독 교체·자동 배치. 작업을 나누고 실패 복구 journal을 보존한다. 자동 배치 등록은 원본처럼 세션 범위다. |
| 명품 관리·자동 배분 | `src/game/specialty_tools.cpp`, `src/overlay/specialty_tools_panel.cpp` | 전체/도시 소유 상태, 이름·설명, 선택/주인공 지급, 평정 뒤 자동 배분. 고정 풀을 재사용하고 한 번에 한 명품씩 반영한다. |
| 내정·교류·보주·도로·도시 전환 | `src/runtime/legacy_feature_control.cpp` 및 기존 `src/game/*` 모듈 | 원본 설정을 유지하면서 27개 작업 그룹을 순환한다. 자동 설정의 P1 준비 후 3초 대기를 보존하고, 수동 최신 요청이 이전 대기 요청을 대체한다. |
| 전투 정책·포로·원군·평정·군주 이동 제안 | `src/runtime/native_feature_control.cpp`, `src/game/native_patch.cpp`, 개별 게임 모듈 | 명령어 확인과 패치 묶음 적용·복구, 대기/적용/실패 상태 게시. 군주 이동 제안의 실제 사용 모드를 연결했다. |
| 전법 편집·전투 환경 | `src/overlay/feature_menu.cpp`, `src/runtime/command_commit.cpp`, 기존 전투 설정 모듈 | 원본 전법 편집 창의 치료·천계·동토·투석·격류/낙석·건물 6개 설정, 날씨·일자·경과일 스킵·지형·공성·회복률을 현재 설정 창에 연결했다. |
| 책략 5칸 | `src/game/stratagem5_native.cpp`, `src/game/stratagem5_native_code.cpp`, `src/runtime/battle_dispatch.cpp`, `src/game/stratagem5_*` | 게임 소유 버튼 할당·ID 7 등록·다섯 번째 버튼 callback·index 4 접근·배치, DATA→COUNT→CAMP→MODEL 연결. 역순 복구 실패 시 뒤 계층을 복구하지 않고 재시도한다. |
| 기재 3슬롯·이름/설명·사용자 정의 | `src/runtime/trait_control.cpp`, `src/game/trait_viewer_native.cpp`, `src/services/trait_catalog_service.cpp`, `src/overlay/trait_text_editor.cpp` | 설정 검증·카탈로그 게시, native 표시, 텍스트 적용·파일 저장을 연결했다. 설치 작업은 다른 controller와 순환한다. |
| 재야·등용·사망 알림 | `src/game/officer_notifications.cpp`, `src/overlay/officer_notice_panel.cpp`, `src/overlay/notifications.cpp` | 재야 12초, 공통 알림 20초와 재야 우선 표시, 유한 대기열·200개 기록. 일반 화면 알림 8개와 기록 200개는 링 저장소를 재사용한다. |
| 주인공 자원·UI 배율 | `src/game/player_resources.cpp`, `src/overlay/feature_menu.cpp` | 금/우호의 증표/담력/특권의 4/2/4/1바이트 접근, 예상 기존 값 비교와 실패 복구. UI 배율은 폰트·레이아웃에 함께 적용하고 새 `fUiScale` 키로 보존한다. |
| 편집 유틸리티 | `src/runtime/debug_tools.cpp`, `src/overlay/debug_tools_panel.cpp` | 노트, 제한된 메모리 보기/쓰기, 증분 검색. 전체 결과를 매 frame 다시 생성하지 않고 보관된 저장소를 재사용한다. |

원본 `BattleUnit` 수집 hook의 두 주소는 편집 메뉴가 아닌 전투 관측용이었다. 현재는 `BattleState` 관측과 `monitor_battle` 연결로 대응한다. 책략의 광범위한 probe dump 및 진단 전용 callback은 기능 경로에서 제외했다.

## 소유권·성능 규칙

### 소유권과 수명

`src/domain/ownership.h`는 `third_party/ownership_cpp`를 사용한다. 큰 작업 상태는 `own::unique_owner`, 읽기 전용 설정·이름 사전은 `own::shared_owner<const T>`로 보관한다. 무장 명단은 생산자와 UI가 각자 고정 저장소를 소유하고, SPSC 채널의 최대 128행 변경 패킷을 UI의 `own::unique_owner`에 반영한다. 소비자에게 내보낸 공유 데이터는 제자리에서 바꾸지 않는다. Windows callback의 raw pointer는 명시적으로 보존한 owner를 빌리는 경계다.

게임 객체의 백업은 epoch와 root·테이블·ID에 묶인다. 실패한 쓰기나 복구의 원본 정보를 성공 확인 전에 지우지 않는다. 세션이 달라진 객체에 이전 세션의 백업을 쓰지 않는다. root/player 슬롯의 읽기 실패는 관측 불가 상태로 처리해 해당 갱신의 작업을 보류하고 epoch·복구 기록을 보존한다. 성공적으로 읽은 null 또는 다른 identity만 세션 전환 근거로 사용한다. 실제 게임의 다른 스레드까지 배타적으로 잠그는 방식은 아니므로, 객체 확인은 원자적인 compare-and-swap을 보장하지 않는다.

종료 시 신규 요청을 닫고 실행 중 작업을 정리한 뒤 데이터와 진입점을 복구한다. 복구가 실패하면 다음 메시지에서 같은 단계를 재시도한다. 실행 중일 수 있는 callback이 돌아올 코드와 소유자는 프로세스 수명까지 유지한다. 창 소실 시에는 추가 게임 쓰기를 중단한다.

### 작업량 제한

| 경로 | 한 번의 처리 상한 또는 재사용 방식 |
|---|---|
| 창 메시지의 전체 갱신 | 공통 1,500µs deadline. Game/Battle/UI/Debug/Specialty/OfficerNames 6개 그룹을 순환하며 그룹당 최대 250µs slice를 남은 전체 예산으로 제한 |
| UI 명령 | 최대 4개. UI 그룹의 남은 deadline을 사용하며 명령 외 서비스도 같은 예산에 포함 |
| 일반 설치 | legacy/native/trait controller 중 한 번에 한 작업 그룹. legacy 내부 27개 그룹을 순환하며 실패별 deadline 적용 |
| AI 성장 | 최대 8명 단위 |
| 특수능력 배정 | 최대 32개 읽기·8개 반영 단위 |
| 도시·후방 지원 | 최대 4개 도시 관측 또는 한 노선 작업 단위 |
| 군단 배치 | 최대 64개 읽기/계산 단위, 쓰기 시도 후 yield, 복구 최대 32개 기록 단위 |
| 명품 | 최대 16개 읽기·600µs 협력 예산, 지급/배분 한 항목 단위 |
| 명단 전달 | 패킷당 최대 128행, 고정 4슬롯 SPSC 채널(유효 3개), Present당 최대 2패킷/256행. 명단·필터 배열 재사용 및 최종 정렬 제거 |
| 실제 무장 이름 | 최대 32개·600µs 협력 예산. 전체 조회는 완료 후 30초 간격, 자녀는 완료 후 2초 간격 |
| 메모리 검색 | 최대 64KiB 블록·500µs 협력 예산, 고정 결과 저장소 재사용 |
| 장기 순회 | 전체 무장 능력·성향·재야는 각 32레코드, 기술 초기화 16레코드. 친밀도 후보 검증·정렬·중복 제거와 가족 수집·페이지 구성도 커서와 deadline으로 재개 |
| 전투 순회 | 전법 횟수 최대 64작업, 지형·책략 치유 각 8유닛, 공성 32타일/8치유유닛. 날짜·전술·metadata 작업도 순환 |
| 주막·군주 보너스·맵 | 주막 16작업, 군주 8작업, 맵 16항목. 조회·적용·복원을 나누고 고정 저장소와 불확정 쓰기 기록을 재사용 |
| 파일 로그 | 고정 4,096개 슬롯, 각 1,024바이트, 서비스에서 최대 64개씩 배출 |
| 입력·알림 | 입력 256개, 서비스 알림 128개, 화면 알림 8개, 기록 200개 저장소 |

각 작업의 기존 항목 상한과 자체 시간 예산보다 공통 deadline이 먼저 적용된다. Game·UI·Battle 내부 작업도 호출 전에 다음 순환 위치를 기록하므로, 긴 작업이 다음 갱신에서 앞 순서를 독점하지 않는다.

시간 예산은 **작업 항목 사이에서 양보하는 조건**이다. 이미 시작한 게임 native 함수, 메모리 작업, 고정 크기 쓰기 트랜잭션은 완료한 뒤 양보한다. 1,500µs는 게임 창 메시지의 작업 admission 예산이며 Present 렌더링·게임 전체 frame의 강제 마감이나 측정된 latency/FPS를 뜻하지 않는다.

RTV, native signature 검색 결과, 코드 버퍼, 작업 배열을 재사용한다. 설정·이름·명품 metadata의 파일 읽기와 문자열 병합은 서비스에서 처리한다. 잠금 내부에서는 작은 상태와 공유 소유자를 교환하고, 큰 데이터의 생성·해제는 잠금 밖에서 수행한다. 짧은 재시도는 메시지·타이머·condition variable의 deadline으로 예약하며 임의 sleep 루프를 추가하지 않는다.

추가한 구현은 명시적인 타입과 `constexpr` 상수·테이블을 사용한다. 구현 내부 약칭을 우선하고, 새 작업의 루프에서 임시 동적 컨테이너를 반복 생성하지 않는다.

## 후속 점검의 네 가지 잔여 구현

| 확인된 잔여 항목 | 반영한 처리 |
|---|---|
| 명령 외 서비스를 포함한 전체 시간 예산 부재 | 공통 `WorkBudget`과 상위/하위 순환 실행을 연결했다. 장기 순회는 커서에서 재개하며 시간 때문에 완료 상태를 앞당기지 않는다. |
| 명단 진행 게시마다 누적 벡터·공유 소유자 재생성 | 고정 ID 인덱스 저장소와 변경 패킷으로 전환했다. 채널이 가득 차면 ACK를 보류하며, 기다리는 사이 같은 행이 다시 바뀌어도 다음 변경분을 잃지 않는다. |
| 책략 버튼 부분 생성 실패의 추적·제한 부재 | Allocate 전에 allocator·raw/button·단계·epoch·generation 기록을 예약한다. 미확정 기록은 최대 64개로 제한하고 한도에 도달하면 새 확장을 보류한다. 확인되지 않은 destructor/free ABI는 호출하지 않는다. |
| Register 성공 후 readback 실패의 영구 미등록 상태 | record당 Register 호출은 한 번으로 제한한다. 후속 이벤트에서 ID 7 lookup, helper 소비, vtable/state, 기존 ID 2–5와 layout generation을 확인해 이미 등록된 자신의 버튼을 채택한다. |

구현 커밋: [버튼 할당·등록 복구](https://github.com/29thnight/S8RPKCheats/commit/74c3473f9bbd105e8031dc38d2f0b27f1528e258), [고정 명단 전달](https://github.com/29thnight/S8RPKCheats/commit/c731b00735363948914cef156e3f27096f97fc30), [공통 예산·작업 재개·복구](https://github.com/29thnight/S8RPKCheats/commit/63059c59281a5201daf55be3e9c0d4d7cd4e1362).

64개 제한은 미확정 기록·할당 시도의 제한이다. 게임 생성자 내부의 총 메모리 바이트 수를 측정하거나 제한한 수치가 아니다. 등록이 확인된 정상 버튼은 generation 종료 때 게임 객체를 직접 해제하지 않고 기록 슬롯만 재사용하므로 정상 UI 재개방 횟수에는 제한을 두지 않는다.

함께 확인한 실패 경로도 수정했다. 주막의 부분 쓰기·복구 실패는 고정 pending pair로 보존하며 종료에서도 재시도한다. 군주 보너스와 맵은 실제 원본을 보존하고 다른 주체의 값을 덮지 않는다. 공성 이동 비용의 복원 필요 상태는 전투 관측 변경만으로 지우지 않는다. 소모형 전법 횟수는 같은 레코드의 쓰기 실패 뒤 재보충하지 않으며 부대 수 변화만으로 첫날 적용을 다시 열지 않는다.

## 설정 호환 및 의도적인 차이

- 원본 키와 별도 파일의 형식을 보존한다. 숫자로 저장된 bool은 0/비0 값으로 읽는다.
- 아직 적용하지 못한 설정도 사용자의 최종 요청 값으로 저장한다. 적용 성공 여부는 별도 상태로 표시한다.
- `fUiScale`은 새 저장 키다. 원본의 폰트 전용 배율과 달리 현재 UI는 폰트와 레이아웃을 함께 확대한다.
- 오래 걸리는 일괄 작업은 진행 상태와 취소를 제공한다. 완료 전의 요청을 즉시 완료된 것으로 표시하지 않는다.
- 책략 5칸과 실제 이름 getter는 원본이 지원한 실행 파일 식별자를 확인한다. 책략 adapter의 timestamp는 `0x69A67ED1`, RSDS GUID는 `B18A027E-19F4-4C35-8749-5B6F0FF814D8`, age는 1이다. 일치하지 않는 빌드에 이 주소들을 강제로 사용하지 않는다.

## 확인한 것과 남은 실행 확인

수행한 검토는 소스 대조, 변경 diff, 공백 오류, 프로젝트 XML의 등록 경로·중복, 직접 호출과 종료 순서의 검토다. 기존 회귀 fixture는 분할 실행·고정 배열·복원 대기 계약에 맞게 수정했으며 실행하지 않았다. 프로젝트에는 필요한 소스 항목을 등록했으며 빌드 도구·컴파일 옵션을 바꾸지 않았다.

실제 Windows/게임 환경에서 확인되지 않은 사항은 다음과 같다.

1. 전체 구성의 컴파일·링크와 각 프록시 DLL의 로딩.
2. 책략 UI callback의 실제 창 스레드 일치 여부, 게임 allocator가 소유한 추가 버튼의 파괴 계약, dialog 재사용 시 동작.
3. 로드·시나리오 전환·데모 중지·가족 편집·도시/군단 대규모 작업·배틀 종료에서의 실제 기능 보존.
4. ResizeBuffers·장치 소실·창 소실·종료 실패 재시도와 게임 callback 경합.
5. 60FPS, 게임의 frame 시간, CPU/GPU 사용량. 제한된 작업량과 재사용 구조는 구현했지만 성능 측정 결과는 없다.

공통 명령·설정·런타임·메뉴 헤더가 바뀌었으므로 Domain, Game, Services, Runtime, Overlay 및 이를 포함하는 Proxy의 관련 번역 단위가 재컴파일 대상이다. 이 문서는 빌드나 테스트를 실행하라는 승인으로 해석하지 않는다.
