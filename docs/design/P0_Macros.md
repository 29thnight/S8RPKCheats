# P0 자체 매크로와 실행 경로 치환

기준: S8RPKCheats `068ca1d881edf870f7cf06f935cf50e286004af8`. 자체 cpp/inc/h를 텍스트 검색하고 아래 wrapper의 include 순서를 수동 확인했다. 빌드 전처리 결과를 생성한 것은 아니다. 전체 매크로 후보는 `p0/source_locations.csv`의 `macro` 항목에 있으며 라이브러리·조건부·주석 후보가 섞여 있다.

| 위치 | 치환 범위와 실제 역할 | 이관 시 보존할 동작 |
|---|---|---|
| `Source.cpp:147` | `Source_impl.inc`의 조기 로드, 브리지 준비, 책략 setter, 초기화, 로그, `Sleep`, `FreeLibraryAndExitThread`를 wrapper로 치환한다. | [시작·종료 조사](P0_Lifecycle.md)의 조기 대기와 worker·로그 종료 순서. |
| `Config.cpp:1` | `ConfigBase.inc`의 Save/Load 이름을 `SaveConfigBase`/`LoadConfigBase`로 바꾼 뒤 별도 wrapper가 추가 필드를 처리한다. | [설정 조사](P0_Config.md)의 순서와 실제 저장 키. |
| `Menu.cpp:278` | `Menu_impl.inc`에서 기존 악명·도시 반란 유지 분기를 억제하고 알림 그리기에 200ms 유지 작업을 연결하며 전투 감시를 `ProfiledMonitorBattleStatus`로 치환한다. | 렌더 유지 작업과 background 감시의 별도 호출 조건. |
| `debug.cpp:147` | `debug_impl.inc`의 `vector`, `thread`, `debuging`을 치환한다. `std::thread(...).detach()`로 보이는 스캐너 호출은 `T05ManagedThread`의 등록 가능한 worker로 바뀐다. | 중복 스캔 억제, 완료 worker 회수, 정상 종료 시 join. |
| `OfficerData.cpp:53` | `OfficerData_impl.inc`의 `VirtualQuery`를 wrapper로 바꾼다. 동일 gameBase의 8MiB 탐색에서 마지막 영역을 조회하면 그 영역의 후보 검사 전에 5초간 다음 탐색 시작을 억제한다. | 주소 변경 시 억제 상태 해제, 마지막 영역에서 성공한 경우와 5초 내 재검색을 구분한다. |
| `SelectOfficercapture_t05_base.inc:12` | 내부 구현의 배우자 검색·창 함수를 legacy 이름으로, 무장 목록 창을 impl 이름으로 치환한 뒤 새 공개 함수를 정의한다. | 공개 배우자 검색은 현재 주인공의 관계 테이블을 동기로 읽고 실패 시 전체 프로세스 스캔을 자동 실행하지 않는다. 무장 목록 wrapper는 refresh 요청을 소비한다. |
| `SelectOfficercapture.cpp:1` | 일괄 랜덤 기재 창 구현 이름을 legacy로 치환하고 공개 wrapper가 실행 직후 runtime 기재 객체 배열을 먼저 확인한다. | 요청 시 객체 선확보와 기존 창 처리 순서. |
| `SelectOfficercapture.h:131,174`, `OfficerDetail.h:2` | include 순서에 따라 `SetTraitID`를 UI 고속 함수로 바꾸며 `TextColored` 호출을 기재 이름 보조 표시 wrapper로 바꾼다. `OfficerDetail.h`는 선언 중 일시적으로 매크로를 해제·복구한다. | 고속 경로 실패 시 분할 검색 요청, 일반 setter와 다른 호출, 특정 출력 형식에서만 커스텀 이름 표시. |
| `TraitConfigRuntime.cpp:6` | `TraitConfigRuntime_impl.inc`의 tick 이름을 내부 구현으로 바꾸고 공개 tick에서 외부 version.dll 정책과 기본 JSON 로드를 먼저 수행한다. | [기재 조사](P0_Traits.md)의 실제 파일 선택과 동작 순서. |
| `StartSetting.cpp:31` | `StartSetting_impl.inc`에서 알림, 미발견 flag와 setter를 legacy guard/함수로 바꾼다. 새 watcher는 별도 공개 setter로 구현한다. | 시나리오 worker와 공개 미발견→재야 watcher를 구분한다. refresh 요청은 include된 구형 알림 wrapper에만 연결되며 공개 watcher의 완료 알림에는 직접 연결되지 않는다. [상세 조사](P0_Tengi_Ronin.md#미발견-전환의-실제-수정-범위와-목록-갱신)를 따른다. |
| `CouncilCompactSection.h:52` | `CouncilCompactSectionBase.h` 안의 `Checkbox`, `IsItemHovered`를 감싼다. 특정 유목기병 체크박스 옆에 단절 영토 이동 제한 옵션을 삽입하고 hover 결과를 보관한다. | 기존 체크박스 tooltip의 hover와 별도 옵션의 setter·저장·실패 상태. |

`NOMINMAX`, `WIN32_LEAN_AND_MEAN`, 헤더 가드, 리소스 ID 및 버전 문자열은 위 동작 재정의와 별도로 분류한다. `S8RPK_STRATAGEM5_DEEP_DIAGNOSTICS` 등 조건부 분기는 실제 구성과 함께 확인해야 한다.

## 자체 정의 목록의 범위

[프로젝트 매크로 정의 후보](p0/project_macro_definitions.csv)는 기준 소스의 cpp/h/inc/c에서 줄 시작 `#define`을 찾은 58개 정의다. `Framework`와 `Hooking`은 외부 라이브러리 조사와 구분해 제외했다. 이 목록은 주석 여부·전처리 조건·include 순서·컴파일러 기본 매크로를 평가하지 않은 텍스트 결과다. 같은 `SetTraitID` 정의가 두 곳에 있고 include 순서에 따라 해제·복구되는 경로가 위 표에 있다. `CouncilCompactSection.h`는 특정 한국어 label의 체크박스를 감싸 옵션을 추가하므로 label 변경도 호출 경로에 영향을 준다. 상세 setter와 12개 패치의 동작은 [기능 보존표](P0_Feature_Contracts.md)의 단절 영토 항목에 기록했다.

값 상수 중 `Source_impl.inc`의 `TRAITS_PTR_OFFSET`은 호출처를 찾지 못한 `ApplyGoldCheat`의 포인터 체인에 사용하고 `WM_IME_FIRST/LAST`는 미정의일 때만 선언한다. `helper.h`의 `DEBUG`는 `_DEBUG`와 이름이 다른 매크로다. 이름 검색과 정의 목록만으로 조건부 경로가 실제 활성화됐다고 판단하지 않는다.

## 책략 5 심층 진단 조건

`StratagemSlotProbe.cpp`의 `S8RPK_STRATAGEM5_DEEP_DIAGNOSTICS` 분기는 선택 함수 코드 바이트·직접 호출 대상과 요약을 기록하고, UI bridge 준비 시 추가 callsite·callback 진단 함수를 호출한다. 매크로 밖의 기본 코드 형태 검사와 조건이 맞을 때의 4→5 패치, 일곱 bridge 준비 호출은 진단 매크로와 별도로 존재한다. 진단을 끄면 다섯 번째 책략 기능 전체가 제거되는 구조는 아니다.

현재 PC에서 hid의 Debug/Release × x64/Win32 네 프로젝트 구성에 대해 해당 컴파일 항목의 `PreprocessorDefinitions`와 `AdditionalOptions`를 읽기 전용으로 평가했다. 네 결과에 이 매크로 정의나 별도 추가 옵션은 없었다. 프로젝트 자체 텍스트에도 정의가 없다. 이는 이번 평가의 항목 메타데이터에 대한 결과이며, 외부 강제 include 또는 다른 전처리 경로까지 분석한 결과는 아니다. 빌드나 전처리를 실행하지 않았다.

## ImGui 설정 헤더와 PCH 의존

`Framework/imgui.h`는 `IMGUI_USER_CONFIG`가 정의돼 있으면 해당 파일을 먼저 포함하고, 이어 저장소의 `imconfig.h`를 포함한다. 현재 `imconfig.h`의 줄 시작 `#define` 두 개(`IM_VEC2_CLASS_EXTRA`, `IM_VEC4_CLASS_EXTRA`)는 모두 블록 주석 속 예제다. FreeType 등 선택 매크로도 주석 처리돼 있다. 따라서 텍스트 색인의 두 예제를 활성 설정으로 집계하지 않는다. 외부 `IMGUI_USER_CONFIG` 또는 컴파일러 명령의 매크로는 이 파일 검사만으로 배제할 수 없다.

`pch.h`는 DX11·DX12·DXGI와 ImGui DX11·DX12·Win32 backend 헤더를 모두 포함하고 관련 DirectX 라이브러리 pragma를 선언한다. 프로젝트는 세 backend cpp도 컴파일 항목에 넣는다. 이는 입력 선언이며 DX12 backend가 현재 게임 렌더 경로에서 사용된다는 뜻은 아니다. 실제 현재 렌더 hook은 [오버레이 조사](P0_Overlay.md)를 따른다. PCH가 사용되는 구성과 별도 항목의 불일치 후보는 [빌드 입력 조사](P0_Build_Inputs.md)에 기록했다.

## 디버그 스캐너 vector 치환의 실제 범위

`debug.cpp:147`에서 `debug_impl.inc`를 포함하는 동안 `vector` 토큰은 `T05DebugVector`로 바뀐다. 이 alias는 `T05DebugAllocator<T>`를 사용하지만, 할당자가 별도 재사용 경로를 택하는 타입은 `unsigned char`뿐이다. `ScanResult`와 메모리 영역 정보 벡터는 같은 alias를 통과해도 내부에서 `std::allocator<T>`를 사용한다. 따라서 매크로가 보인다는 이유로 모든 검색 결과 벡터가 바이트 풀에 들어간다고 해석하면 안 된다.

최초 검색의 블록 버퍼와 주변 재검색의 바이트 버퍼는 할당 시 현재 스레드 풀에 충분한 블록이 있으면 꺼내고, 해제 시 풀보다 큰 블록을 하나 보유한다. 더 작은 반환 블록은 즉시 해제한다. 풀은 `thread_local`이며 소멸자가 보유 블록을 해제한다. 매크로로 치환된 검색 작업은 관리형 스레드에서 실행되므로 해당 작업 스레드가 끝나면 그 스레드의 풀도 해제된다. 별도 검색 worker 사이에 이 풀이 자동 공유되는 구조는 아니다.

`debuging` wrapper는 완료한 관리 worker를 회수한 뒤 원래 창 함수를 호출한다. 종료 시 stop 요청과 join 순서의 경합은 [작업 스레드 조사](P0_Workers.md#디버그-스캐너-정지-요청-순서)를 따른다. 회귀 조건은 최초·주변 검색 결과 동등성, 블록 크기 변경, 작업 종료 뒤 재검색, 중지와 부분 결과, 완료 worker 회수 및 종료다. 메모리 검색이나 빌드는 실행하지 않았다.

## 유사한 기재 포인터 상수의 서로 다른 경로

`Source_impl.inc`의 `TRAITS_PTR_OFFSET=0x57A4B8`은 이름과 달리 `ApplyGoldCheat`가 읽는 체인에만 사용된다. 이 함수는 실행 파일 시작 주소의 첫 포인터를 읽고 해당 상수 위치의 포인터를 따라 `+0x40`에 금액을 쓰지만, 기준 자체 cpp·h·inc의 텍스트 검색에서 정의 외 호출은 찾지 못했다. 이 상수를 활성 기재 runtime의 배열 오프셋으로 취급하지 않는다. 외부에서의 호출 가능성은 검색 범위 밖이다.

활성 `TraitConfigRuntime_impl.inc`는 다른 기본값 `0x57A4D0`을 사용한다. `ResolveTraitsPointerOffset`은 첫 호출에 `offsetResolved=true`와 기본값을 먼저 기록하고, 실행 파일 이미지에서 패턴을 찾았을 때 허용 범위의 후보 오프셋으로 바꾼다. 모듈 정보 조회나 패턴 탐색이 실패해도 이번 소스에서 `offsetResolved`를 false로 되돌리는 대입은 찾지 못했다. 따라서 같은 runtime 상태에서는 이후 tick에서 패턴을 다시 찾지 않고 기본값을 계속 사용하는 경로다.

반면 무장 UI 고속 기재 변경과 일괄 배정 wrapper는 `0x57A4D0`을 고정값으로 사용한다. runtime이 패턴에서 다른 값을 선택한 경우 이 UI 경로가 runtime의 선택값을 읽는 코드는 해당 함수에 없다. 빠른 조회 실패 시 UI는 별도의 비동기 기재 변경 요청을 등록한다. 각 값이 실제 게임에서 가리키는 객체와 버전별 결과는 확인하지 않았다.

회귀 조건은 호출되지 않는 금 함수와 활성 기재 경로의 구분, 패턴 첫 검색 실패 후 준비 완료, 다른 유효 후보 오프셋, 고정 오프셋 UI와 runtime의 차이, fallback 검색 및 게임 세대 전환이다.

## 관계 테이블 탐색 억제의 설정 시점

`T05OfficerDataVirtualQuery`는 `gameBase`부터 8MiB 범위의 마지막 메모리 영역을 조회할 때, 호출자 `TryResolveSynergeticTable`이 그 영역을 읽고 후보를 검사하기 **전에** 5초 억제 상태를 기록한다. 따라서 마지막 영역에서 유효 후보를 찾아 탐색이 성공해도 억제 상태는 설정될 수 있다. 후보가 캐시로 계속 유효하거나 고정 오프셋에서 발견되면 후속 호출은 억제 검사를 거치지 않는다. 반대로 그 경로들이 실패하고 같은 base에서 5초 안에 fallback을 다시 시작하면 첫 `VirtualQuery`가 0을 반환해 검색이 중단된다. 마지막 영역이 첫 영역과 같아도 같은 순서다.

억제는 gameBase 주소를 기준으로 하므로 같은 주소의 새 게임 세대를 구분하지 않는다. 다른 gameBase를 사용하면 이전 주소에 대한 억제 조건이 맞지 않으며, 정상 `VirtualQuery` 반환 후 기록을 지운다. 실제 메모리 영역 구성과 이런 재검색의 발생 여부는 확인하지 않았다. 회귀 조건은 마지막 영역에서 성공 후 캐시 무효화, 같은 주소의 세대 교체, 다른 주소로 전환, 5초 경계와 고정 오프셋 성공이다.
관계 조회의 직접 호출은 군단 추천, 도시 친밀도 진단, 무장 관계 표시, 임신 배우자 목록 및 AI 친밀도 일괄 처리에서 확인된다. 상생 테이블 fallback만 억제돼도 직접 관계 테이블을 찾으면 일괄 조회는 성공하고 상생 정보 없이 결과를 반환할 수 있다. 양쪽 테이블이 모두 해석되지 않는 경우 군단 추천은 관계 묶기를 건너뛰고, 친밀도 진단과 AI 친밀도 처리는 중단하며, 임신 배우자 갱신은 실패를 반환한다. 무장 관계 화면은 선택 주소가 바뀔 때 조회 결과를 캐시하므로 그 주소가 유지되는 동안 실패나 부분 결과를 다시 읽지 않는 경로다. 이는 확인된 직접 호출의 정적 분기이며 실제 빈도나 화면 결과는 확인하지 않았다. 관계 조회 자체의 부분 성공은 [기능 보존표](P0_Feature_Contracts.md#군단-추천에서-관계-조회-실패)를 따른다.

## 압축 평정 메뉴의 삽입 체크박스

`CouncilCompactSection.h`의 wrapper는 원래 유목기병 체크박스의 변경 결과와 hover를 먼저 보관하고 단절 영토 체크박스를 그린 다음 원래 변경 결과를 반환한다. 따라서 삽입 항목 자체의 변경 결과가 유목기병 setter의 조건으로 전달되지는 않는다. 원래 구현의 다음 `IsItemHovered`는 보관한 유목기병 hover를 한 번 반환하며, 삽입 항목의 tooltip은 wrapper 안에서 별도로 처리한다. 삽입 위치는 `SameLine(160.0f)`로 고정되어 있고 압축 메뉴의 다른 열 위치에 쓰는 `160.0f * scale`과 다르다. 서로 다른 scale에서 실제 배치·hover는 실행하지 않았다.
