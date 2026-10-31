# P0 작업 스레드와 설치 수명

기준: `068ca1d881edf870f7cf06f935cf50e286004af8`. 정적 소스에서 직접 확인한 생성과 관리 경로다. 스레드 핸들을 닫는 것은 작업 완료를 기다리는 동작이 아니다. 명시한 종료 경로 외 호출과 게임 callback 실행 시점은 별도 추적·실행 검증 대상이다.

| 작업 | 생성·실행 | 보관·중지·완료 |
|---|---|---|
| MainThread 초기화 | `dllmain.cpp:28`, `dllmain_dxgi.cpp:39`, `dllmain_hid.cpp:62`의 선택된 진입점 | 생성 직후 핸들을 닫는다. MainThread는 정상 경로에서 `FreeLibraryAndExitThread`에 도달한다. |
| 초기화 재시도와 ClientBGThread | `Source_impl.inc:331,366`에서 MainThread가 `std::thread` 생성. 후자는 `Menu::Loops`를 30ms마다 호출한다. | MainThread 지역 변수로 소유하며 `g_Running` 종료 후 두 스레드를 join한다. |
| 파일 로그 | `showlog.cpp:113`에서 모듈 참조를 얻고 `_beginthreadex`로 생성한다. | 핸들과 모듈 참조를 보관한다. `ShutdownFileLogWriter`는 새 입력을 막고 깨운 뒤 기다리고 핸들·모듈 참조를 해제한다. |
| 디버그 스캐너 | `debug_impl.inc:388,393`의 `std::thread(...).detach()` 표기는 `debug.cpp:147`의 매크로에 의해 `T05ManagedThread`로 치환된다. | wrapper의 `detach()`는 실제로 관리 worker를 등록한다. `ShutdownDebugScannerT05`가 정지 요청 후 보관된 스레드를 join한다. |
| 대도시·방목·농경·상업 도시 변경 | 각 `Bigcityconvert.cpp:48`, `BangmokCity.cpp:41`, `NonggyeongCity.cpp:40`, `SangeopCity.cpp:41`의 요청당 worker. ON은 두 차례 200ms 대기 후 게임 데이터를 쓴다. | 각 atomic 실행 표시가 중복 요청을 막는다. 핸들은 생성 직후 닫는다. 같은 기능 worker가 실행 중이면 새 요청은 그대로 반환한다. |
| 내정 배율 설치 | `DomesticsMult.cpp:174`에서 패턴 탐색과 code cave 설치 worker 생성. | 핸들을 즉시 닫는다. OFF는 저장한 원본 9바이트를 복원하고 cave를 해제한다. 설치 worker와 OFF 사이의 완료 대기는 이 경로에 없다. |
| 기술 포인트 캡처 | `Techpointcave.cpp:62`에 패턴 탐색 및 설치 worker가 있다. 조사한 cpp·h·inc에서는 이 setter의 호출을 찾지 못했다. | 호출되면 핸들을 즉시 닫는다. OFF는 캡처 주소를 지우고 설치 상태면 원본 7바이트 복원과 cave 해제를 수행한다. [연결 상태 조사](P0_Feature_Contracts.md)를 따른다. |
| 상극·충성·공명 | `InstantLoveCave.cpp:180`, `Loyaltycave.cpp:64`, `Resonancecave.cpp:195`에서 패턴 탐색과 code cave 설치. | 앞의 두 worker는 핸들을 닫고 공명은 `std::thread`를 detach한다. OFF는 각 cave 복원을 시도한다. 공명은 두 설치 중 실패하면 `RemoveResonanceHooks`를 호출한다. |
| 전투 유닛 캡처 | `Battleunitcapture.cpp:252`의 worker가 설치 가능 전투 시점까지 100ms씩 기다리고 패턴을 찾는다. | 생성 핸들은 즉시 닫는다. OFF는 설치 요청을 내리고 주소를 지우며 설치됐으면 원본 코드 복원·cave 해제를 한다. |
| 전기 캡처 | `TengiCave.cpp:129`의 worker가 설치 작업을 수행한다. | 핸들은 닫는다. OFF는 요청 취소 후 설치 상태면 원본 7바이트를 복원하고 cave를 해제한다. 설치 worker를 기다리는 경로는 없다. 자세한 내용은 [전기 조사](P0_Tengi_Ronin.md)를 따른다. |
| 기술 초기화 | `Techzero.cpp:153`에서 종료 이벤트와 worker 핸들을 소유한다. | OFF에서 이벤트를 알리고 스레드를 기다린 후 핸들과 이벤트를 닫는다. MainThread 정상 종료도 OFF를 호출한다. |
| 미발견 무장 감시 | `StartSetting.cpp:236`에서 이벤트와 감시 worker를 만든다. | `StartSetting.cpp:255`에서 종료를 기다리고 핸들을 닫는 경로가 있다. MainThread 정상 종료에서 OFF를 호출한다. |
| 시나리오 설정 | `StartSetting_impl.inc:552`에서 이벤트와 worker를 만든다. | OFF에서 이벤트를 알리고 기다린다. MainThread 정상 종료에서 OFF를 호출한다. |
| 도로·군주 보너스 | `Roadblock.cpp:393`, `FactionLordBonus.cpp:302`에서 이벤트와 worker를 만든다. | OFF에서 이벤트를 알리고 기다린다. MainThread 정상 종료에서 각각의 비활성화가 호출된다. |
| 특수능력 자동배정 | `SpecialAbilityAutoAssign.cpp:281`은 `rosterBase`를 캡처해 분리 스레드에서 각 무장 정보를 읽고 결과를 mutex 아래 게시한다. | cancel·pause atomic은 있으나 스레드 객체는 detach된다. 게임 세대 변경과 DLL 종료 시 작업 완료는 별도로 확인해야 한다. |
| 기재 일괄 배정 대상 수집 | `SelectOfficercapture_impl.inc:991`의 분리 worker가 무장 배열을 읽고 주소 벡터를 mutex 아래 게시한다. | 수집한 값은 게임 메모리 주소다. 결과 사용과 새 게임·로드의 관계 확인이 필요하다. |
| 배우자 검색 | `SelectOfficercapture_impl.inc:2938`의 분리 전체 메모리 스캐너는 include 매크로에 의해 legacy 이름으로 바뀐다. 공개 `StartSpouseScannerAsync`는 `SelectOfficercapture_t05_base.inc:389`에서 관계 테이블을 동기로 조회한다. | legacy 구현이 파일에 존재한다는 것과 공개 창에서 실행된다는 것을 구분한다. [매크로 조사](P0_Macros.md)를 따른다. |

도시·code cave worker와 게임 메모리 수명은 변경 시점의 스레드 안전성이 확인되지 않았다. 새 구현에서는 안전한 게임 실행 문맥의 근거를 얻기 전까지 원시 게임 주소를 백그라운드 작업으로 옮기지 않는다. 위 기록은 기존 동작의 근거이며 새 구현이 자동으로 기존 비동기 동작을 재현해야 한다는 뜻은 아니다.



## 파일 로그 writer의 큐와 소유

`showlog.cpp:109`의 `StartFileLogWriter`는 파일 로그가 OFF여도 시작 시 모듈 참조를 얻고 `_beginthreadex` worker를 만든다. `AddLog`는 디버그와 파일 로그가 모두 OFF면 반환하며, 디버그 화면은 mutex 아래 최근 1000개를 유지한다. 파일 로그는 생산자 시각을 붙여 UTF-8로 정규화한 뒤 최대 4096건·4MiB의 큐에 넣는다. writer가 없거나 중지 중이거나 큐 제한을 넘으면 항목을 버리고 drop 카운터를 증가시킨다.

writer는 큐를 별도 batch로 교환한 뒤 잠금을 놓고 현재 작업 디렉터리의 `S8RPK_cheat.log`에 추가한다. 빈 파일에는 UTF-8 BOM을 쓰고 batch마다 flush한다. 파일 쓰기 실패는 해당 batch 건수를 drop 카운터에 더하고 파일을 닫으며 다음 batch에서 다시 열기를 시도한다. 성공적으로 기록된 항목이 batch 앞부분에 있었더라도 batch 전체를 drop으로 세는 경로가 있다.

정상 종료의 `ShutdownFileLogWriter`는 중지 flag를 세워 새 생산을 거부하고 worker를 깨워 대기 중 큐를 처리한 다음 스레드를 기다리고 핸들과 추가 모듈 참조를 해제한다. 대기 실패 시 `std::terminate`를 호출하며, 중지 flag 때문에 재시작하지 않는다. DllMain detach에서는 이 join 경로를 호출하지 않는다. 실제 큐 포화·파일 오류·종료와 다른 로그 생산자의 경합은 실행하지 않아 확인되지 않았다.

## 도시 변환 요청과 중복 토글

`SetBigCityConvert`와 `SetBangmokCity`는 각 atomic 실행 flag를 `exchange(true)`로 올리고, 이미 작업 중이면 새 ON/OFF 요청을 즉시 버린다. worker는 시작 시 전달받은 bool을 한 번 읽고 도시 배열 포인터를 해석한다. ON은 200ms씩 두 번 대기하며 타입과 한도를 단계적으로 쓰고, OFF는 고정된 타입과 한도를 쓴다. 첫 포인터 해석 실패 시 실행 flag를 내리고 끝난다. 생성된 스레드 핸들은 즉시 닫아 호출자가 완료를 기다리지 않는다. 스레드 생성이 실패하면 flag는 내리지만 `new bool(enable)`로 넘긴 인자를 회수하는 경로는 없다.

이 setter는 `void`이므로 공통 설정 적용 루프는 worker의 생성·완료·실제 쓰기 성공을 받지 못한다. ON 작업 중 OFF 요청이 들어오면 setter는 이를 버릴 수 있고, 공통 루프는 OFF 호출 전에 자체 적용 flag를 내리므로 나중에 자동으로 다시 OFF를 시도하지 않는 경로가 있다. 작업의 두 단계는 처음 해석한 주소를 재사용하고, 이 소스에는 중간 취소나 게임 세대 재확인 및 종료 시 join이 없다. `VirtualProtect`의 성공 여부도 확인하지 않는다. 농경·상업 도시 함수는 별도 소스로 동일 여부를 각 항목에서 확인해야 한다.

회귀 조건은 작업 중 ON→OFF와 OFF→ON, worker 생성 실패, 포인터 해석 실패, 두 대기 사이의 로드·종료, 메모리 보호 변경 실패와 부분 쓰기다. 위는 정적 흐름이며 실제 경합 빈도나 게임 실행 결과는 확인하지 않았다.

농경·상업 도시 변경도 각각 독립 atomic flag, 요청당 `CreateThread`, 작업 중 새 요청 버리기, 핸들 즉시 닫기, worker 생성 실패 시 bool 인자 미회수 구조를 사용한다. ON 두 단계 사이 포인터를 다시 해석하지 않으며 `VirtualProtect` 결과를 확인하지 않는다. 각 실행 flag는 기능별로 따로 존재하므로 서로 다른 도시 기능의 동시 실행을 막지 않는다.

상업도시의 15개 오프셋에는 대도시 전환의 7개 오프셋 전부가 포함된다. 두 worker가 같은 도시 타입과 개발·상업 한도를 서로 다른 값으로 쓸 수 있다. 상업 ON의 첫 단계는 타입 128을 유지하도록 검사하지만, 대도시 ON과 상업 ON/OFF의 실행 순서에 따라 최종 한도는 달라질 수 있다. 이관 확인에는 같은 기능의 중복 토글뿐 아니라 대도시·상업 기능의 네 ON/OFF 조합과 두 단계의 교차 실행을 포함한다. 실제 동시 실행 빈도는 측정하지 않았다.

네 도시 변환 소스의 정적 대상 오프셋을 [쌍별 중복 목록](p0/city_conversion_overlaps.csv)으로 대조했다. 선언된 배열 기준으로 대도시 7개는 상업도시 대상에 모두 포함되며, 나머지 다섯 쌍은 동일한 타입 오프셋이 없다. 이는 네 배열끼리의 비교이며 도시 일괄 편집이나 다른 기능의 쓰기와 충돌하지 않는다는 증거는 아니다. 겹치는 7곳에서 대도시 ON은 개발/상업 한도 9000/12000, 상업 ON은 5000/6500을 쓰며 두 작업은 별도 worker다.

도시 목록 편집의 네 한도 주소는 도시 배열 기준 각각 `+0xD0/+0xD4/+0xD8/+0xDC`로 계산된다. 도시 변환도 자기 도시 기준에 같은 네 오프셋을 쓴다. 다만 두 기능의 포인터 체인이 다르므로 두 기준 주소가 실제로 같은 배열이라는 사실은 정적 코드만으로 확정하지 못했다. 두 기준이 같다는 가정 아래 `(타입 오프셋 - 0x10) / 0x2A0`으로 계산한 [도시 인덱스 후보](p0/city_conversion_index_candidates.csv)를 기록했다. 네 목록의 모든 오프셋이 이 계산에서 정수 인덱스가 되지만, 이것만으로 게임에서 주소가 같음을 증명하지 않는다. 실제 동일 배열이면 개별 편집·일괄 최대화와 변환 worker가 같은 한도를 쓸 수 있으므로 주소 관계와 순서를 확인해야 한다.

## 특수능력 자동배정 worker와 결과 적용

`SpecialAbilityAutoAssign.cpp:406`은 작업 시작 시 무장 배열 주소를 한 번 해석하고 ID 중복을 걸러 사용할 대상 정보를 분리 worker에 넘긴다. 연간 모드는 기존 특수능력이 있는 ID를 worker 시작 전에 `g_skillCountMutex` 아래 스냅샷으로 제외한다. worker는 캡처한 배열 주소를 사용해 최대 5102 슬롯을 순회하고 결과를 별도 mutex 아래 게시한다. `std::thread`는 detach되며 이 함수에서 완료를 기다릴 핸들을 보관하지 않는다.

`TickSpecialAbilityAutoAssign`은 연간 모드의 화면 상태에 따라 pause를 설정하고 취소 요청을 worker의 atomic cancel에 전달한다. worker는 각 슬롯 앞에서 취소를 확인하고 pause 중 10ms씩 기다린다. 생성 시 pause는 false로 초기화되므로 첫 tick이 pause를 설정하기 전 worker가 진행할 수 있는 코드 순서다. worker 완료 후 tick이 취소를 확인하면 결과를 적용하지 않고 종료한다. 취소가 없으면 결과를 가져와 `g_customSkillCounts`를 mutex 아래 ID별로 갱신하고 새 항목이 있으면 `SaveSkillCounts`를 호출한다. 작업 시작 시 캡처한 배열 주소와 결과 적용 시 게임 세대가 같은지 검사하는 코드는 이 경로에 없다.

연간 모드에서 tick이 안전하지 않은 화면을 감지하면 pause를 설정하지만, worker 완료가 이미 게시된 경우에는 안전 상태가 될 때까지 결과 적용을 기다리는 분기가 없다. 취소가 아니면 같은 tick에서 `FinishAutoAssignJob`을 호출한다(`SpecialAbilityAutoAssign.cpp:584-633`). 완료 적용과 연말 알림 기록 추가는 분리 worker가 아니라 `Menu::Loops`의 ClientBGThread에서 실행된다. 따라서 pause는 worker의 다음 슬롯 읽기 제어와 결과 적용의 안전 조건을 동일하게 보장하지 않는다. 화면 전환과 완료의 실제 순서는 확인하지 않았다.

신규 배정 시 호출하는 `SaveSkillCounts`는 성공 여부를 반환하지 않는다. 그 뒤 작업은 완료 상태와 결과 알림을 설정하므로 완료 표시는 저장 성공의 증거가 아니다. 특히 자동배정의 맵 직접 변경은 변경 version을 증가시키지 않아 저장 실패 후 지연 저장이 재시도하지 않는 경우가 있다. 파일과 version 경로는 [수치 파일 조사](P0_Resources.md#무장-전법특수능력-수치-파일)를 따른다.

연간 모드는 시작할 때 이미 특수능력이 하나라도 있는 무장의 ID를 스냅샷으로 제외한다. 완료 적용 시에는 각 후보 능력 키가 현재 양수인지 확인하지만, 그 무장에게 다른 종류의 특수능력이 새로 생겼는지는 다시 검사하지 않는다. 따라서 시작 후 수동으로 능력 B를 추가한 무장이 worker에서 능력 A의 후보로 남아 있다면, 적용 시 B가 있어도 A를 추가할 수 있다. 반대로 시작 때 제외된 무장의 기존 능력을 작업 중 지워도 그 작업의 후보에 다시 넣지 않는다. 두 접근은 맵 mutex로 보호되지만 시작과 적용 사이의 의미상 변경까지 같은 스냅샷으로 묶지는 않는다. 실제 편집과 작업의 겹침은 확인하지 않았다.

회귀 조건은 시작 직후 안전하지 않은 화면, 취소 전파 전후 완료, 연간 작업의 기존 능력 스냅샷과 작업 중 변경, 배열 교체·주소 재사용, worker 진행 중 종료와 저장 실패다. 실제 동시 실행 순서나 종료 안전성은 게임에서 확인되지 않았다.

## 생성 API 추가 대조

기준 트리의 자체 cpp·h·inc에서 `CreateThread`, `_beginthreadex`, `std::thread`, `std::jthread`, `CreateThreadpoolWork`, `QueueUserWorkItem` 문자열이 있는 파일을 조사 문서의 파일명과 대조했을 때 이름이 전혀 없는 파일은 없었다. 별도 검색에서 `std::async`, `_beginthread`, `CreateRemoteThread`, `CreateThreadpool*`, `QueueUserWorkItem`, `RegisterWaitForSingleObject`, `SetTimer` 호출 문자열은 발견되지 않았다. Framework와 Hooking 폴더는 이 대조에서 제외했다.

이는 파일명과 텍스트 검색 범위에 한정된 결과다. 같은 파일의 모든 생성 경로가 표에 있는지, 매크로·간접 호출·외부 라이브러리의 작업이 무엇인지, 각 callback이 실제 어느 스레드에서 실행되는지는 증명하지 않는다. 기술 포인트 캡처는 생성 함수의 존재와 실제 호출 경로를 구분해 위 표를 수정했다.

## 디버그 스캐너 정지 요청 순서

`debug.cpp`의 `T05PrepareDebugWorkerStart`는 스캔 중 flag를 먼저 설정하고 stop을 false로 만든 뒤 관리 스레드를 시작한다. 정상 종료 wrapper의 `ShutdownDebugScannerT05`는 stop을 true로 설정하고 등록된 worker를 가져와 join한다. 그런데 worker 본문 `InternalFirstScan`과 `InternalNextScan`도 시작할 때 stop을 다시 false로 쓴다. 따라서 종료 측이 stop을 세운 뒤 worker 본문이 시작하는 순서에서는 정지 요청이 지워지고 join이 검색 완료까지 기다릴 수 있다. 정지 여부는 첫 검색의 영역·블록 경계, 다음 검색의 결과별 반복에서 확인한다.

스레드 시작 후 등록 전에 종료가 worker 목록을 가져가는 순서도 코드상 따로 직렬화하지 않는다. 실제 종료와 UI 호출의 스레드 순서가 그 상황을 허용하는지는 확인되지 않았다. 완료 worker 회수는 `debuging` wrapper 호출 시 이루어지며 정상 종료에서도 남은 목록을 join한다. 이 내용은 가능한 코드 순서의 정적 분석이며 실제 경합 재현 결과가 아니다.

### 자동배정 상태의 UI·background 공유

수동 시작·취소 및 진행 창 조회는 `MenuSectionsOfficerEdit.cpp`의 렌더 경로에서 호출하고, `TickSpecialAbilityAutoAssign`과 연간 시작은 `Menu::Loops`의 background 경로에서 호출한다. `AutoAssignJob`의 `running`, `cancelRequested`, `cursor`, `status`, `resultLines`는 일반 bool·정수·문자열·벡터이며 이 접근을 공통으로 감싸는 잠금은 해당 함수들에서 보이지 않는다. worker 완료 결과의 별도 mutex와 atomic 진행·중지 flag가 이 UI/job 상태까지 보호하는 것은 아니다. 실제 동시 접근 순서와 재현 여부는 확인하지 않았다.

연간 worker의 pause 판정 `IsAnnualAutoAssignSafeState`는 주석의 내정만이라는 설명과 달리 gameState `0x05` 또는 `0x07`을 허용한다. 안전 상태 probe는 100ms 캐시하며 worker 시작 시 pause는 false이고 다음 background tick이 설정한다. 회귀 조건은 수동 시작·취소와 background 완료의 겹침, 결과 문자열을 그리는 중 결과 교체, 연간 시작과 수동 시작의 경쟁, 0x05·0x07·전환 상태 및 종료다.

## 배속 시간 원본과 공유 시계

`SpeedHack.cpp`는 QPC·GetTickCount·GetTickCount64·timeGetTime 각각의 실제·가상 기준값을 전역 atomic으로 따로 보관한다. hook은 직접 호출자의 반환 주소가 이 DLL 범위에 있거나 `SpeedHack_Update`를 처음 호출한 관리 스레드이면 원본 시간을 돌려준다. DLL 내부에서 외부 라이브러리를 거쳐 API를 호출할 때 반환 주소가 외부 모듈이면 이 직접 호출자 검사만으로 제외되지 않는다. 모듈 범위를 얻지 못해도 `IsCheatCaller`는 false다. 관리 스레드는 첫 호출의 thread ID로 한 번 정해지며, UI와 background 양쪽에 호출 경로가 있어 어느 쪽이 먼저인지 소스만으로 고정되지 않는다.

배속 ON·OFF 전환 또는 배율 변경은 네 시계의 기준을 0으로 초기화하고 다음 호출을 새 기준으로 삼는다. QPC는 실제 간격이 50ms를 넘으면 해당 간격의 배율을 1로 낮추고, 계산된 간격은 33ms로 제한한다. 나머지 세 API도 실제 간격이 50ms를 넘으면 그 간격은 1배를 사용한다. atomic 개별 load/store는 동시 호출의 전체 읽기·계산·쓰기 순서를 묶지 않으므로 여러 게임 스레드의 실제 시간 연속성은 별도 확인이 필요하다. `SpeedHack_GetRealDeltaTime`은 원본 QPC 포인터를 사용해 알림 이동 시간에 배속을 적용하지 않으려 한다.

회귀 조건은 첫 관리 스레드 선택, DLL 직접·외부 경유 호출, 모듈 범위 확인 실패, 33·50ms 경계와 긴 정지, 다중 스레드 동시 호출, 배율 변경과 ON/OFF 전환, 원본 QPC 실패다. 실행 시간은 측정하지 않았다.

## 스레드 생성 텍스트 후보 재대조

기준 저장소의 자체 cpp·h·inc·c에서 `std::thread`, `std::jthread`, `std::async`와 `CreateThread`, `_beginthreadex`, `CreateRemoteThread`, `QueueUserWorkItem`, `CreateThreadpoolWork`, `SubmitThreadpoolWork`의 직접 표기를 다시 검색했다. 외부 `Framework`, `Hooking`과 `legacy`는 제외했다. [검색 후보](p0/thread_creation_candidates.csv)는 25개 파일의 32개 줄이며 25개 파일명 모두 이 문서에 등장한다.

32줄은 스레드 32개를 뜻하지 않는다. `debug.cpp`에는 멤버 선언, 인자 선언·정의, 주석이 포함되며 `debug_impl.inc`의 두 호출은 `T05ManagedThread` 매크로 치환을 거친다. 세 프록시 진입점도 선택한 구성에서 하나만 컴파일된다. 배우자 legacy 경로는 별도 매크로 치환 때문에 공개 UI에서의 호출 여부를 앞 표와 같이 구분한다. 별도로 `SetTimer`, timer queue, thread-pool timer, APC, fiber 등의 지정한 이름을 검색한 범위에는 일치가 없었다.

이는 텍스트 색인의 누락 후보를 줄인 결과다. 매크로 별칭·함수 포인터·간접 호출, 라이브러리 내부 작업, Windows가 호출하는 callback 및 실제 동시 실행과 소유자는 이 검색만으로 확정되지 않는다. 빌드와 게임 실행은 하지 않았다.

## 일괄 기재 배정의 작업 소유와 게시 순서

`SelectOfficercapture_impl.inc:1350`의 실행 버튼은 렌더 경로에서 새 job을 만들고 기재 후보와 기준 객체를 먼저 확보한 다음 무장 수집 worker를 시작한다. 공개 wrapper `SelectOfficercapture.cpp`는 기존 창 함수가 반환된 직후 런타임 기재 포인터 배열에서 후보를 추가 확보한다. worker는 자신의 실행 시점에 roster 배열을 별도로 해석하므로, 화면 측 기재 객체 확보 시점과 roster 해석 시점은 같지 않다. 두 결과에 공통 게임 세대 번호를 붙여 비교하는 코드는 이 작업에 없다.

worker는 최대 5102개 슬롯을 검사해 유효 ID 1~5102의 첫 항목만 raw 주소로 수집한다. 결과 벡터를 mutex 아래 교체한 후 `collectRunning=false`, `collectDone=true` 순서로 atomic 상태를 바꾼다. 렌더 tick은 완료 flag를 본 뒤 같은 mutex 아래 벡터를 옮겨 job에 보관하고 완료 flag를 내린다. 빈 결과면 작업을 종료한다. 이후 별도의 렌더 tick에서 저장된 주소에 배정한다. atomic 완료 표시는 벡터 게시와 수집 완료를 연결하지만 주소가 같은 게임 세대에서 유효함을 보증하지 않는다.

이 worker는 detach되고 종료 신호와 join 경로가 없다. 수집 중에는 각 슬롯의 ID와 일부 접근 가능성을 확인하지만 수집이 끝날 때까지 게임이 로드되거나 종료되지 않도록 붙잡지 않는다. 게임 로드 전 기재 객체와 로드 후 roster를 조합하거나 반대로 조합할 가능성은 코드의 시점 차이에서 도출한 조건이며 실제 주소 재사용과 경합은 확인되지 않았다. 회귀 조건은 시작과 worker 첫 실행 사이의 로드, 수집 도중 로드, 완료 게시 후 렌더 tick 재개, 중복 ID, 빈 결과, 종료 중 worker와 게임 주소 수명이다.

### 배속 시계별 초기화와 32비트 경계

네 시간 API는 가상 기준을 서로 공유하지 않고 각 API의 첫 대상 호출에서 실제 값을 그대로 반환하며 자체 실제·가상 기준을 설정한다. ON/OFF 또는 배율 변경 후 기준이 0으로 초기화되므로 다음 대상 호출의 기준은 이전 가상값이 아니라 그때의 실제값이다. 따라서 전환 전 가상 시간이 실제 시간과 달랐다면 전환 뒤 값이 연속된다고 이 코드만으로 보장할 수 없다. DLL 내부 직접 호출과 최초 관리 스레드는 각 hook에서 실제 시간을 받으므로 같은 API라도 호출 주체에 따라 반환 기준이 다르다.

`GetTickCount`와 `timeGetTime`은 `DWORD` 차이를 계산한 뒤 이를 `int32_t`로 해석해 음수이면 증가량을 0으로 만든다. 가상값 덧셈도 32비트다. `GetTickCount64`는 실제 값이 이전 값보다 작으면 증가량을 0으로 하고 64비트 가상값을 쓴다. QPC는 음수 차이를 0으로 하며 배율 적용 후 한 번의 증가량을 QPC 주파수 기준 33ms로 제한한다. 나머지 세 API에는 이 33ms 상한이 없고, 네 API 모두 실제 차이가 50ms를 초과하면 해당 차이에 1배를 적용한다.

회귀 조건은 네 API의 첫 호출과 전환 직후, 호출 주체별 실제·가상 시간, 32비트 wrap과 큰 간격, 50ms 경계, QPC 33ms 상한 및 동시 호출이다. 실제 게임 시계나 호출 빈도는 측정하지 않았다.

### 배속 훅 등록 충돌 시 롤백 대상

기준에 포함된 `Hooking/hook.c:834`의 `MH_CreateHookApiEx`는 대상 API를 찾으면 `MH_CreateHook` 호출 전에 `ppTarget`에 주소를 저장한다. 이 MinHook 인스턴스에 같은 대상이 이미 등록돼 있으면 `MH_CreateHook`은 `MH_ERROR_ALREADY_CREATED`를 반환한다. `SpeedHack_Sleep_Install`은 이 반환을 실패로 처리하고 `RollbackSpeedHooks`를 호출한다.

롤백은 네 대상 변수 중 0이 아닌 모든 주소에 대해 `MH_DisableHook`과 `MH_RemoveHook`을 호출한다. 따라서 실패한 대상이 같은 MinHook 인스턴스의 기존 등록이었다면, 배속 기능이 새로 생성하지 않은 그 등록도 비활성화·제거를 시도한다. 호출 결과는 확인하지 않는다. 별도의 MinHook 인스턴스나 다른 방식의 훅까지 같은 등록부에 있다는 뜻은 아니다. 실제 충돌이나 제거 성공은 실행하지 않아 확인하지 않았다.

회귀 조건은 네 API 중 각 위치에 기존 등록이 있는 경우, 앞 단계의 신규 등록과 뒤 단계 충돌, 비활성화·제거 실패 및 다른 기능의 trampoline 수명이다.

## 도시 변경과 내정 배율 worker 생성 실패의 차이

대도시·방목·농경·상업 도시 변경의 네 setter는 `CreateThread`가 실패하면 각 atomic 실행 표시를 false로 되돌린다. 성공하면 핸들을 닫고 worker가 종료 시 표시를 내린다. 이 처리는 생성 실패 시 재요청을 막는 표시가 남지 않는다는 범위의 소스 동작이다.

반면 내정 배율 `DomesticsMult.cpp:180-210`은 일반 bool인 `g_domesticsRunning`을 true로 바꾼 후 스레드를 생성하고, 성공한 핸들만 닫는다. 생성 실패 시 실행 표시를 되돌리는 분기가 없다. 이 차이와 설정 flag의 불일치는 [설정 조사](P0_Config.md)의 내정 배율 항목에서도 다룬다. 회귀 조건은 네 도시 작업과 내정 배율 각각의 스레드 생성 실패, 재요청, 설치 중 OFF 및 종료다. 스레드 생성 실패를 실행해 재현하지 않았다.

내정 배율의 메뉴 `MenuSectionsDomestic.cpp:215`는 렌더 경로에서 `SetDomesticsMult`를 호출하고, 설정 자동 적용은 `ConfigBase.inc`의 setter 표를 통해 background loop에서 같은 함수를 호출한다. 설치 worker는 일반 bool인 `g_domesticsApplied`, `g_domesticsRunning`을 읽거나 쓴다. 이 상태와 hook 주소·cave 주소에 대한 공통 mutex나 atomic은 해당 구현에 없다. OFF가 설치 완료 전에 실행되면 적용 flag가 아직 false여서 복원 분기를 건너뛰고, worker는 이후 설치를 계속할 수 있는 코드 순서다.

배율 슬라이더는 `g_playerMult`·`g_forceMult`를 갱신하며 생성된 cave는 이 변수들의 주소에서 배율을 읽는다. cave는 게임 호출 문맥에서 실행되므로 메뉴·설정 갱신과 실제 호출의 스레드 및 동시성은 확인하지 않았다. `ResetDomesticsPlayer`는 기준 자체 cpp·h·inc의 직접 표기에서 정의·선언 외 호출을 찾지 못했다. 이 사실이 외부·간접 호출을 배제하지는 않는다.

추가로 같은 자체 소스 범위에서 `_beginthread`, `CreateRemoteThreadEx`, `CreateThreadpool`, `TrySubmitThreadpoolCallback`, `CreateTimerQueueTimer`, `QueueUserAPC`, `SetWinEventHook`, `SetWindowsHookEx`의 직접 호출 표기를 검색했지만 일치가 없었다. 이는 지정한 이름의 텍스트 검색으로, 매크로 치환·함수 포인터·외부 라이브러리가 만드는 작업이나 게임의 callback 스레드를 배제하지 않는다.

## 작업 종료 대기 결과의 차이

기술 초기화, 시나리오 설정, 미발견 전환 watcher, 군주 보너스, 도로 worker의 OFF 경로는 stop event를 알린 뒤 보관한 스레드에 무기한 대기한다. 다섯 경로 모두 `WaitForSingleObject` 반환값을 검사하지 않고 스레드 핸들과 stop event를 닫고 보관 위치를 비운다(`Techzero.cpp:206`, `StartSetting_impl.inc:580`, `StartSetting.cpp:255`, `FactionLordBonus.cpp:330`, `Roadblock.cpp:417`). 따라서 반환 자체만으로 작업 종료가 확인됐다고 기록하지 않는다. 앞의 파일 로그 writer는 대기 실패 시 `std::terminate`하는 별도 동작이다. 실제 대기 실패나 작업 중 핸들 종료는 실행하지 않았다.

미발견 전환과 도로 worker의 시작 함수는 기존 스레드 핸들을 0ms 대기해 `WAIT_TIMEOUT`이면 실행 중으로 취급한다. 그 밖의 반환값에는 종료 신호와 `WAIT_FAILED`가 모두 포함되지만, 두 경우를 구분하지 않고 기존 핸들·이벤트를 닫고 새 worker 생성을 시도한다(`StartSetting.cpp:215-236`, `Roadblock.cpp:373-393`). 실제 대기 실패 여부나 그때 기존 worker가 실행 중인지는 확인하지 않았다. 군주 보너스와 기술 초기화의 시작 경로도 남은 핸들을 무기한 기다린 뒤 결과 확인 없이 닫지만, 각각 앞의 활성·실행 상태 조건을 먼저 검사한다.

종료 이벤트 자체의 대기가 `WAIT_FAILED`일 때 위 다섯 worker는 모두 로그 후 루프를 벗어난다. 기술 초기화 worker는 이 종료 경로에서 `g_techZeroEnabled`를 내리지 않으며, 이후 같은 ON setter는 이 flag가 true면 바로 반환하므로 재시작하지 않는다. 시나리오 설정 worker는 `g_startSettingRunning`을 false로 내리지만 enabled와 requested가 유지되면 ON setter는 running을 보지 않고 바로 반환한다. 군주 보너스 worker는 running을 false로 내리므로 이후 ON setter가 다시 호출되면 남은 핸들을 정리하고 재시작 경로에 들어갈 수 있다. 이는 setter가 다시 호출된다는 뜻이 아니다. 도로 worker의 선택 flag 유지와 재시작 조건은 해당 기능 조사에 기록했다. 실제 대기 실패는 확인하지 않았다.
