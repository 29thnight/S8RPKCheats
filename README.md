# S8RPKCheats

삼국지 8 REMAKE with 파워업키트용 인게임 치트·편집 오버레이입니다. [WOOSEOK99/S8RPKCheats](https://github.com/WOOSEOK99/S8RPKCheats)의 기능을 C++23 구조로 이관하고, 게임 리소스를 사용하는 UI와 편집 작업 흐름을 구성한 [29thnight 포크](https://github.com/29thnight/S8RPKCheats)입니다.

문서 기준: 2026-11-02 로컬 작업본. 커밋하지 않은 변경을 포함합니다.

## 주요 기능

원본의 무장·도시·내정·교류·전투·성장 기능을 바탕으로 실행 구조와 메뉴 구성을 변경했습니다.

| 메뉴 | 기능 |
|---|---|
| 무장 | 무장 정보·관계 조회, 능력·전법·특기 편집, 군단 배치 |
| 기재/성장 | 기재 효과 6개의 종류·값·P, 이름·설명 편집, AI 성장·랜덤 기재, 전법·특기 일괄 편집 |
| 도시/내정 | 도시 자원·개발 한도·종류 변경, 수송 노선, 세력 기술, 내정 설정 |
| 평정/교류 | 평정·교류 관련 설정과 보주 편집 |
| 전투 | 날씨·일자·지형·공성·치유, 전법·건물, 포로·원군 관련 설정 |
| 시나리오 | 날짜·규칙·데모 중지, 자녀 조기 임관·임신 슬롯·배우자 관리 |
| 환경설정 | UI 배율·시작 설정, 바로가기·알림·로그, PK 중복 편집기 잠금 해제 |

원본의 고정 커밋 [`068ca1d`](https://github.com/WOOSEOK99/S8RPKCheats/commit/068ca1d881edf870f7cf06f935cf50e286004af8)과의 상세 차이는 [원본 비교 문서](docs/design/Upstream_Comparison_2026-11-01.md)와 [기능 대응표](docs/design/Implementation_Parity.md)에 정리했습니다.

## 설치

생성한 `hid.dll`을 게임 실행 파일과 같은 폴더에 복사합니다. 다른 프록시 구성을 빌드했다면 해당 구성에서 생성한 DLL을 사용합니다. DLL 이름만 바꿔서 프록시를 전환할 수는 없습니다.

폰트와 게임 UI는 설치된 게임 데이터를 직접 읽으므로 별도의 게임 UI PNG 폴더는 필요하지 않습니다. 범위 미리보기 그림 11장은 DLL에 포함합니다.

## 메뉴와 조작

- 백틱(`) 키: 편집 도구 열기·최소화.
- 화면의 원형 붓 버튼: 편집 도구 열기·최소화. 왼쪽 드래그로 위치를 이동하며 위치는 실행 중 유지됩니다.
- 붓 버튼 우클릭: 바로가기 메뉴.

PK 기본 편집기로 처리할 수 있는 무장·주인공 등의 직접 편집은 기본 화면에서 숨기며, 환경설정에서 편집 기능을 잠금 해제하면 사용할 수 있습니다.

전법·특기 일괄 편집은 버튼을 누를 때 `변경 제외 → 0 → 1 → 2 → 3 → 변경 제외` 순서로 바뀝니다. 우클릭은 해당 항목을 변경 대상에서 제외합니다. 노란 막대는 전법, 청록 막대는 특기 숙련도이며, **전체 무장에 적용**을 눌러 반영합니다.

도시 종류·한도 변경은 도시 목록에서 대상을 체크하고 변환 종류를 선택한 뒤 적용합니다.

사용자 설정 파일은 `S8RPK_cheat_config.json`입니다. 기재 편집은 현재 로드된 기재 JSON을 사용하며, 외부 편집기의 기재 설정 파일은 `san8r_traits_config.json`입니다.

## 게임 리소스와 VERSION.dll

폰트와 게임 UI 리소스는 설치된 게임 폴더의 `root.rdb`·`root.rdx` 및 관련 데이터에서 읽습니다. 추출한 게임 UI PNG는 저장소와 DLL에 포함하지 않습니다. 게임 UI를 읽지 못하면 기본 컨트롤을 사용합니다. 별도 범위 미리보기 PNG 11장은 DLL에 내장하며, 범위 선택은 기존 그림 버튼으로 표시합니다. 리소스 대응 내역은 [개발 문서](docs/design/UI_Workbench_2026-11-01.md)에서 확인할 수 있습니다.

`VERSION.dll`이 있으면 외부 편집기가 관리하는 기재와 보조 보주를 구분해 처리합니다.

- 기재 효과: 설정 파일을 저장한 뒤 확인된 외부 편집기의 설정 로드 명령을 전달합니다.
- 보조 보주: 지원 대상으로 식별된 외부 DLL의 읽기·설정 함수에 연결합니다.
- 병종·품관·관직·도시 등 F8 종합편집기 전체 항목을 우리 UI에서 편집하는 연결은 아직 완성하지 않았습니다.

이 연결은 분석한 SAN8RPK 편집기 v1.4.7의 코드 경로를 기준으로 합니다. 임의의 다른 `VERSION.dll`을 같은 인터페이스로 취급하지 않습니다.

## 빌드

Visual Studio 2026, MSVC v145와 Windows SDK 10.0.26100.0을 사용합니다. 공통 컴파일 설정은 [S8.Cpp23.props](build/S8.Cpp23.props)에 있습니다.

```powershell
git clone --recurse-submodules https://github.com/29thnight/S8RPKCheats.git
cd S8RPKCheats
```

이미 받은 저장소는 `git submodule update --init --recursive`로 의존성을 준비합니다.

[S8RPKCheats.sln](S8RPKCheats.sln)을 열고 다음 구성 중 하나를 선택합니다. 플랫폼은 모두 x64입니다.

| 솔루션 구성 | 생성 DLL | 출력 폴더 |
|---|---|---|
| Debug / Release | `hid.dll` | `out/x64/Debug` / `out/x64/Release` |
| Debug-DXGI / Release-DXGI | `dxgi.dll` | `out/x64/Debug` / `out/x64/Release` |
| Debug-DInput8 / Release-DInput8 | `dinput8.dll` | `out/x64/Debug` / `out/x64/Release` |

각 구성은 선택한 프록시와 해당 의존 프로젝트를 빌드합니다. 테스트 프로젝트는 자동 빌드에서 제외했습니다. DLL 이름만 바꾸면 다른 프록시로 전환되는 구조가 아닙니다.

Developer PowerShell에서 HID Debug만 빌드하려면 다음 명령을 사용합니다.

```powershell
msbuild src/proxy/S8.Hid.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

## 확인 현황

현재 소스에 연결된 기능과 게임에서 실제 확인한 동작은 구분합니다. 특히 최신 메뉴 통합, 선택 도시 변환, 군단 조회 시간 변경은 게임 재확인이 남아 있습니다. 보주 편집과 외부 기재 효과 적용도 게임에서 적용 결과를 확인해야 합니다.

독립 미리보기는 샘플 데이터로 UI를 확인하는 도구입니다. 실제 게임 메모리 변경 여부를 검증하지 않습니다. 실행·스크린샷·입력 확인 방법은 [미리보기 안내](tools/preview/README.md)에 있습니다.

## 출처와 개발 문서

원본 프로젝트: [WOOSEOK99/S8RPKCheats](https://github.com/WOOSEOK99/S8RPKCheats). 실행 골격 참고: [NightFyre/DX11-ImGui-Internal-Hook](https://github.com/NightFyre/DX11-ImGui-Internal-Hook). 원본 기준 소스는 `reference/S8RPKCheats`에 고정되어 있습니다.

게임 폰트·텍스처는 게임 리소스이며, 포크 자체 제작 리소스와 구분합니다. 의존 라이브러리·리소스 해석 코드의 출처는 [third_party 안내](third_party/README.md)에 있습니다.

- [설계 문서](docs/design/S8RPKCheats_Cpp23_Runtime_Design.md)
- [기능 대응표](docs/design/Implementation_Parity.md)
- [원본 대비 상세 비교](docs/design/Upstream_Comparison_2026-11-01.md)
- [UI 구성과 스프라이트](docs/design/UI_Workbench_2026-11-01.md)
- [P0 조사 기록](docs/design/P0_Audit.md)
- [기존 실행 경로와 조사 자료](docs/design/P0_Inventory.md)
