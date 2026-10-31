# P0 MinHook 대상과 정적 소유 목록

기준 `reference/S8RPKCheats` 커밋 `068ca1d881edf870f7cf06f935cf50e286004af8`. 자체 cpp·inc·h에서 `MH_CreateHook`, `MH_CreateHookApi`, `MH_CreateHookApiEx`의 직접 표기를 대조하고 호출되는 wrapper와 반복문을 확인했다. 실행 결과가 아니라 코드상 등록 대상과 소유 경로다. 한 호출 지점이 여러 대상을 만드는 경우를 분리한다.

| 등록 대상 | 설치 요청·조건 | 개별 정리 또는 수명 | callback 실행 주체 |
|---|---|---|---|
| DX11 Present 1개 | `Engine.cpp:234`, MainThread의 `HookD3D` → `Hooking::CreateHook` | 실패 시 생성된 등록을 제거하지 않는 경로. 종료 시 전체 MinHook 처리. | 게임의 Present 호출 스레드. 실제 ID·동시성 미확인. |
| DX12 ExecuteCommandLists 1개 후보 | `Engine.cpp:336`, DX12 장치 분기에서 임시 큐 vtable을 통한 같은 wrapper | 호출 및 실제 대상 장치 사용 여부 미확인. 현재 DX11 Present 경로와 구분. | 해당 API 호출 스레드; 실제 실행 미확인. |
| `GetCursorPos`, `GetAsyncKeyState`, `GetKeyState`, `GetKeyboardState`, `PeekMessageW/A` 6개 | `Source_impl.inc:354`, MainThread가 등록 후 전체 활성화 | `oGetCursorPos` 하나가 null일 때만 여섯 등록과 전체 활성화를 시도하며 각 반환값을 검사하지 않는다. 정상·detach 경로의 전체 비활성화와 별개로 callback 완료 대기는 확인되지 않음. | 각 API 호출 스레드; 실제 ID 미확인. |
| QPC, GetTickCount, GetTickCount64, timeGetTime 4개 | `SpeedHack.cpp:304`, 배속 활성 시 설치 | 일부 실패 시 생성 대상별 롤백 시도. OFF와 전체 종료는 [배속 조사](P0_Workers.md#배속-시간-원본과-공유-시계) 참조. | 각 API 호출 스레드. 최초 관리 스레드 판정 별도. |
| 보조 보주 판정 1개 | `JewelSettings.cpp:257`, 선택 활성화 또는 저장된 마스크 적용 시 원본 확인 | 활성화 실패 시 제거. 선택 전체 OFF는 hook 자체 제거가 아님. | 게임 판정 호출 스레드; 실제 ID 미확인. |
| 기재 이름·설명·효과 조회 3개 | `TraitConfigRuntime_impl.inc:487,784,1033`, 외부 version.dll이 없는 runtime tick | 실패한 대상 제거, 후속 설치 순서와 table 적용 조건은 [상세 조사](P0_Details.md#기재-runtime의-개별-minhook-대상과-조건-패치) 참조. | 각 게임 getter·효과 호출 스레드; 실제 ID 미확인. |
| 기재 호환 대상 5개 | 같은 파일 `EnsureExtendedTraitCompatibility`가 공통 설치 함수를 5회 호출 | 보유 판정·대화 메시지·효과 데이터·월별 처리·전근 연출별 독립 상태. 부분 실패 시 일괄 롤백 없음. | 각각의 게임 호출 스레드. 효과 연결 캐시는 thread_local; [기능 조사](P0_Traits.md#확장-기재-효과-판정과-호환-hook) 참조. |
| 기재 이름 편집 연결 1개 | `TraitTextNameHook.cpp:576`, 외부 version.dll이 없고 편집 행이 있을 때 기존 runtime detour에 연결 | OFF는 표만 비우고 연결 hook은 유지. 외부 DLL이 있으면 이 경로 대신 직접 포인터 교체. | 원본 이름 getter 호출 스레드; 실제 ID 미확인. |
| 전기 허용 필터 3개 | `TengiCave.cpp:614`, background tick에서 준비·원본 확인 뒤 배열 순회 | 일부 실패 시 생성분 비활성화·제거 시도. 설치 자체 실패 후 자동 재시도 조건은 [전기 조사](P0_Tengi_Ronin.md) 참조. | 각 게임 조건·목록·발생 함수 호출 스레드; 실제 ID 미확인. |
| 주인공 추적 1개 후보 | `Cheats.cpp:493` 설치 함수 정의. 기준 자체 소스에서 호출처 미발견 | 활성 callback의 주소 캡처가 주석. 현재 활성 기능으로 집계하지 않음. | 외부 호출 및 실행 여부 미확인. |

표의 26개는 코드상 대상 역할의 합(조건부 DX12와 미연결 후보 포함)이며 실제 동시에 설치된 hook 수가 아니다. `InstallCompatibilityHook`의 5회, 전기 루프의 3회, 입력·시간 API 호출을 각각 펼친 수다. 주소 별칭이나 MinHook의 중복 등록 실패도 이 합계에서 판정하지 않는다. 직접 JMP·callsite 수정, 실행 코드 cave, WndProc 교체와 데이터 쓰기는 별도 목록인 [직접 패치 조사](P0_War_Patches.md), [오버레이 조사](P0_Overlay.md)를 따른다. Kiero의 라이브러리 내부 구현, 매크로·함수 포인터를 통한 추가 호출 및 실제 callback 스레드 ID는 이 표의 정적 검색만으로 확정하지 않는다.

개별 `MH_DisableHook(target)`과 전체 해제는 포함된 MinHook에서 스레드 명령 위치를 처리하는 action이 다르다. 특히 개별 해제 후 제거 시 trampoline 수명에 대한 정적 분석은 [시작·종료 조사](P0_Lifecycle.md#minhook의-스레드-정지와-개별-해제)를 따른다. MinHook의 일시 정지는 callback 완료를 기다리는 별도 quiescence 계약이 아니다.
## 공통 wrapper 호출 위치 재대조

자체 `.cpp`, `.inc`, `.h`에서 `Hooking::CreateHook(` 표기를 다시 확인한 결과, 정의를 제외한 호출은 `Engine.cpp:237`의 Present와 `Engine.cpp:336`의 DX12 `ExecuteCommandLists` 두 곳이다. 이 wrapper는 `MH_CreateHook` 성공 후 `MH_EnableHook`을 호출하고 둘 중 하나라도 실패하면 false를 반환한다. 두 호출자는 반환값을 검사하지 않는다. Present 쪽 `HookD3D`는 hook 등록 성공과 관계없이 `bInit`을 true로 설정하고 성공을 반환한다. DX12 쪽은 임시 큐를 해제하고 false를 반환한다. 이는 해당 이름의 직접 텍스트 호출을 확인한 결과이며 동적 호출이나 라이브러리 내부까지 증명하지 않는다. 회귀 조건은 생성 실패와 활성화 실패 각각에 대한 초기화 상태, 재시도 및 정리다.
