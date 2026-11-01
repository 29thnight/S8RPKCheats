# S8RPKCheats C++23 런타임 작업 공간

Visual Studio 2026에서는 [S8RPKCheats2026.sln](S8RPKCheats2026.sln)을 연다. 새 구현, 프록시, 의존 프로젝트, 테스트와 계획·참고 자료를 솔루션 탐색기에서 확인할 수 있다. 테스트 프로젝트는 Debug x64만 제공하므로 Release 솔루션 구성에서는 빌드 대상에서 제외된다.

클론할 때는 `git clone --recurse-submodules https://github.com/29thnight/S8RPKCheats.git`을 사용한다. 이미 클론했다면 `git submodule update --init --recursive`로 참고 소스와 의존성을 가져온다.

- [설계 문서](docs/design/S8RPKCheats_Cpp23_Runtime_Design.md)
- [P0 조사 현황과 미완료 항목](docs/design/P0_Audit.md)
- [기존 실행 경로와 조사 자료 안내](docs/design/P0_Inventory.md)
- [기능 보존표](docs/design/P0_Feature_Contracts.md)
- [현재 기능 대응](docs/design/Implementation_Parity.md)
- [원본 대비 조사·세부 기능 이관·설정 로딩 수정](docs/design/Upstream_Comparison_2026-11-01.md)
- [분류형 편집 UI와 스프라이트 적용](docs/design/UI_Workbench_2026-11-01.md)
- [게임 없이 ImGui 창 실행·스크린샷·입력 검증](tools/preview/README.md)

`reference/S8RPKCheats/`는 기존 소스의 독립 Git 클론이며 기준 커밋 `068ca1d881edf870f7cf06f935cf50e286004af8`을 참고용으로 고정했다. `reference/NightFyre/`는 실행 골격 출처 비교에 사용하는 별도 클론이다.

현재 구현은 `domain`, `game`, `runtime`, `services`, `overlay`, `platform/windows`, `proxy`로 분리되어 있다. 게임 읽기·쓰기, 메뉴 명령, 설정 저장, DX11 오버레이와 hook의 연결 코드가 있으며 공통 C++23 설정은 [속성 시트](build/S8.Cpp23.props)에 있다. 구현 경로의 존재와 실제 게임에서의 동작 검증은 구분한다.

[5번 책략 값 처리](src/domain/stratagem5.cpp)는 [native adapter](src/game/stratagem5_native.cpp)와 [전투 갱신](src/runtime/battle_dispatch.cpp)에 연결되어 있다. 버튼의 게임 소유권·파괴 계약과 실제 실행 확인은 남아 있다. 비교 조사에서 누락으로 확인한 세력 색상 편집, 주인공·선택 무장의 관계 표, 배우자 관계 순서 교환과 이름에서 상세로 이동하는 기능은 후속 구현으로 연결했다. 사용 위치와 데이터 검증·복구 범위는 위 비교 문서에 기록했다.

## 독립 ImGui 미리보기

`tools/preview`는 실제 편집 UI 코드를 공유하는 별도 실행 프로그램이다. 게임을
실행하지 않고 8개 분류 화면, 스프라이트, 편집창과 적용 상태 표시를 확인한다.
상단에서 정상 연결·주인공 없음·설정 로딩·적용 오류·외부 관리 샘플을 바꾸고,
배율 조정과 PNG 저장을 할 수 있다. 미리보기의 편집은 샘플 데이터에만 적용된다.
Windows 실행 패키지 사용법, 소스 빌드와 재현 가능한 입력 검증 명령은 위 안내에 있다.

P0 문서는 초기 조사 기록이다. 현재 기능 상태는 기능 대응·비교 문서를 함께
확인한다. 앞선 소스 조사·설정 로딩 수정·세부 기능 이관 당시에는 빌드·테스트·게임
실행을 하지 않았다. 후속 UI 작업에서는 사용자의 빌드·스크린샷·동작 검증 요청에
따라 독립 미리보기를 빌드하고 검증했다. 이 결과는 실제 게임 메모리 동작 검증과
구분한다.
