# S8RPKCheats C++23 런타임 작업 공간

Visual Studio 2026에서는 [S8RPKCheats2026.sln](S8RPKCheats2026.sln)을 연다. 새 구현, 프록시, 의존 프로젝트, 테스트와 계획·참고 자료를 솔루션 탐색기에서 확인할 수 있다. 테스트 프로젝트는 Debug x64만 제공하므로 Release 솔루션 구성에서는 빌드 대상에서 제외된다.

클론할 때는 `git clone --recurse-submodules https://github.com/29thnight/S8RPKCheats.git`을 사용한다. 이미 클론했다면 `git submodule update --init --recursive`로 참고 소스와 의존성을 가져온다.

- [설계 문서](docs/design/S8RPKCheats_Cpp23_Runtime_Design.md)
- [P0 조사 현황과 미완료 항목](docs/design/P0_Audit.md)
- [기존 실행 경로와 조사 자료 안내](docs/design/P0_Inventory.md)
- [기능 보존표](docs/design/P0_Feature_Contracts.md)

`reference/S8RPKCheats/`는 기존 소스의 독립 Git 클론이며 기준 커밋 `068ca1d881edf870f7cf06f935cf50e286004af8`을 참고용으로 고정했다. `reference/NightFyre/`는 실행 골격 출처 비교에 사용하는 별도 클론이다.

새 구현은 [기재 값 모델](src/domain/trait.h), [ID 값](src/domain/ids.h), [오류 결과](src/domain/error.h)와 [도메인 프로젝트](src/domain/S8.Domain.vcxproj)를 분리한 단계다. 공통 C++23 설정은 [속성 시트](build/S8.Cpp23.props)에 있다. 기존 게임 읽기·쓰기에는 연결하지 않았으며 빌드나 실행으로 검증하지 않았다. `schemas/`, `resources/`, `third_party/`는 후속 작업을 위한 자리다.

[5번 책략 값 처리](src/domain/stratagem5.cpp)는 기존 설정의 기본값·정규화, 레코드 필드 변경과 일치 판정을 값 연산으로 이관했다. 게임 메모리, 설정 파일, UI와 hook은 아직 연결되지 않았다.

P0는 진행 중이다. 빌드 입력과 기존 동작을 조사하고 있으며 실행 경로·출처·스레드 소유자의 미확인 항목은 P0 현황에 남겨두었다. 빌드 및 테스트는 요청 시 진행한다.
