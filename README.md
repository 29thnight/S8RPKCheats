# S8RPKCheats

삼국지 8 REMAKE with 파워업키트용 인게임 치트·편집 오버레이입니다. [WOOSEOK99/S8RPKCheats](https://github.com/WOOSEOK99/S8RPKCheats)의 기능을 C++23 구조로 이관하고, 게임 리소스를 사용하는 UI와 편집 작업 흐름을 구성한 [29thnight 포크](https://github.com/29thnight/S8RPKCheats)입니다.

> README 초안 · 2026-11-02 작업본 기준. 아래 비교는 원본의 고정 커밋 [`068ca1d`](https://github.com/WOOSEOK99/S8RPKCheats/commit/068ca1d881edf870f7cf06f935cf50e286004af8)과 현재 로컬 구현을 기준으로 합니다. 아직 커밋하지 않은 변경도 포함합니다.

## 원본과 달라진 점

원본의 무장·도시·내정·교류·전투·성장 기능을 바탕으로 실행 구조와 메뉴 구성을 변경했습니다. 기존에 있던 기능을 모두 새로 추가한 것으로 소개하지 않으며, 세부 이관 내역은 [원본 비교 문서](docs/design/Upstream_Comparison_2026-11-01.md)와 [기능 대응표](docs/design/Implementation_Parity.md)에 정리했습니다.

| 영역 | 원본 기준 | 현재 포크 |
|---|---|---|
| 코드 구조 | `Internal DX11 Base`를 중심으로 메뉴·게임 접근·실행 코드 구성 | `domain`, `game`, `runtime`, `services`, `overlay`, `platform/windows`, `proxy`로 분리 |
| UI와 게임 처리 | 기존 메뉴에서 기능별 처리 호출 | UI는 스냅샷을 표시하고 명령을 제출하며, 게임 조회·변경은 런타임에서 처리 |
| 메뉴 | 기존 열 단위 치트 메뉴와 보조 편집창 | 개요·무장·기재/성장·도시/내정·평정/교류·전투·시나리오·환경설정으로 분류 |
| UI 리소스 | 기존 ImGui 메뉴 | 게임 폰트 아틀라스·버튼·도판·초상화·지도 사용, 배경 흐림과 게임 화면에 맞춘 패널 구성 |
| 기재 편집 | 기본 기재·기재 관련 기존 기능 | 효과 6개의 종류·값·P, 이름·설명을 같은 화면에서 편집하고 성장·랜덤 배정도 같은 분류에서 처리 |
| 도시 관리 | 기존 도시 편집·고정 도시군 변환 | 도시 목록·수송·세력 기술을 도시/내정 화면에 통합하고, 사용자가 체크한 도시의 종류·한도를 변경하는 명령 추가 |
| 일괄 전법·특기 | 기존 일괄 편집 기능 | 병종·분야별 버튼과 3칸 숙련도 표시로 편집 대상을 구성한 뒤 전체 무장에 적용 |
| 가족·시나리오 | 기존 가족·날짜·데모 관련 기능 | 시나리오 왼쪽에 날짜·규칙·데모 중지, 오른쪽에 자녀 조기 임관·임신 슬롯·배우자 관리 배치 |
| 군단 배치 | 기존 군단·무장 이동 기능 | 단계별 작업·진행 수 표시, 무장 조회와 정렬을 32개 단위로 처리 |
| 외부 편집기 | 외부 `VERSION.dll`의 별도 창 | 확인된 기재 설정 로드 경로와 보조 보주 함수에 연결. 종합편집기 전체 탭 연동은 미완료 |
| 빌드 | 원본 솔루션과 프록시별 진입점 | 솔루션 하나에서 HID·DXGI·DInput8을 구성으로 선택. 공통 C++23 설정과 vcpkg 자동 링크 중복 방지 적용 |
| UI 확인 | 게임 실행 화면 | 실제 UI 코드를 공유하는 독립 미리보기 추가. 샘플 데이터로 화면·입력을 확인 |

## 메뉴와 조작

- 백틱(`) 키: 편집 도구 열기·최소화.
- 화면의 원형 붓 버튼: 편집 도구 열기·최소화. 왼쪽 드래그로 위치를 이동하며 위치는 실행 중 유지됩니다.
- 붓 버튼 우클릭: 바로가기 메뉴.
- **기재/성장**: 기재 효과·텍스트, AI 성장·랜덤 기재, 전법·특기 일괄 편집.
- **도시/내정**: 왼쪽 도시 관리, 오른쪽 내정 설정. 도시 목록에서 대상을 체크하고 변환 종류를 선택해 적용합니다.
- **시나리오**: 날짜·데모 중지·시나리오 규칙과 자녀·배우자 관리.

PK 기본 편집기로 처리할 수 있는 무장·주인공 등의 직접 편집은 기본 화면에서 숨기며, 환경설정에서 편집 기능을 잠금 해제하면 사용할 수 있습니다.

전법·특기 일괄 편집은 버튼을 누를 때 `변경 제외 → 0 → 1 → 2 → 3 → 변경 제외` 순서로 바뀝니다. 우클릭은 해당 항목을 변경 대상에서 제외합니다. 노란 막대는 전법, 청록 막대는 특기 숙련도이며, **전체 무장에 적용**을 눌러 반영합니다.

## 게임 리소스와 VERSION.dll

폰트와 UI 리소스는 게임 폴더의 `root.rdb`·`root.rdx` 및 관련 데이터에서 읽습니다. 개발 PC의 수동 추출 폴더를 설치 경로로 요구하지 않습니다. DLL 내장 리소스도 함께 사용하며, 리소스 대응 내역은 [개발 문서](docs/design/UI_Workbench_2026-11-01.md)에서 확인할 수 있습니다.

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

생성한 DLL은 게임 실행 파일과 같은 폴더에 복사합니다. 기본 출력은 `hid.dll`입니다. 사용자 설정 파일은 `S8RPK_cheat_config.json`이며, 기재 편집은 현재 로드된 기재 JSON을 사용합니다. 외부 편집기의 기재 설정 파일은 `san8r_traits_config.json`입니다.

## 확인 현황

2026-11-02 작업본의 HID Debug x64 빌드는 완료했습니다. 컴파일 경고가 있으며, 이 결과는 Release·다른 프록시 빌드나 게임 동작 확인을 포함하지 않습니다.

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
