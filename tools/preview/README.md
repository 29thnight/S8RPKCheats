# 게임 없이 ImGui 창 확인하기

`s8_ui_preview`는 **게임에서 사용하는 실제 메뉴와 편집창 소스**를 SDL 창에
표시하는 독립 실행 프로그램입니다. `src/overlay/feature_menu.cpp`, 기재·무장·도시
등 기존 편집창, `menu_sprites.h`, `UiBridge`, `CommandMailbox`, 저장소의 ImGui 코어를
그대로 컴파일합니다. 게임 런타임, 주입 코드, 메모리 조회 코드는 링크하지 않습니다.

창에 표시되는 장수·도시·적용 상태는 샘플입니다. 체크박스, 배율, 일부 무장/도시 필드와
기재 텍스트는 샘플 메모리에 반영됩니다. 지원하지 않는 게임 작업은 실행하지 않았다고
알려줍니다. 게임 세이브나 기존 설정 파일을 읽거나 바꾸지 않으며 창을 닫으면 샘플
편집값이 사라집니다. 이 프로그램의 통과 결과는 게임 패치의 성공을 의미하지 않습니다.

## 실행

Windows 배포 ZIP을 받았다면 압축을 푼 뒤 `s8_ui_preview.exe` 또는
`start-preview.cmd`를 실행합니다. SDL DLL, `resources`, `fonts` 폴더를 함께 둡니다.
추가 콘솔 창 없이 UI 창만 열립니다.

기본 편집 창은 100% 배율에서 **900 × 620**, 왼쪽 분류는 **170px 폭 / 34px 행**입니다.
분류 사이에 빈 버튼 간격을 두지 않고 선택한 행만 강조합니다. 리소스 분석과 변경
내용은 [작은 편집 창과 최소화 아이콘](../../docs/design/UI_Compact_2026-11-01.md)에 있습니다.

창의 **−**, 제목 표시줄의 접기 또는 **X**를 누르면 제목줄 대신 작은 목록 아이콘만
남습니다. **아이콘 클릭** 또는 **백틱(`)**으로 직전 분류·스크롤·위치·크기를 복원합니다.
아이콘 우클릭에서는 설정한 알림/주인공/모든 무장 바로가기를 사용할 수 있습니다.
이미 별도로 연 편집창은 독립적으로 사용할 수 있습니다. 설정 팝업이 열려 있을 때
백틱을 누르면 팝업을 닫은 후 최소화합니다.

배포 ZIP의 `start-minimized.cmd` 또는 `--auto-load`는 실제 시작 설정 경로로
작은 아이콘부터 표시합니다. 실행 중인 게임을 사용하지 않습니다.

창 상단의 **상태** 메뉴로 다음 화면을 전환할 수 있습니다.

| 샘플 | 확인할 내용 |
|---|---|
| 시나리오 연결 | 주인공 및 공용 데이터가 준비된 화면 |
| 타이틀 · 주인공 없음 | 주인공 편집은 비활성, 공용 무장/기재/도시는 별도 사용 |
| 설정 불러오는 중 | 설정 준비 전의 비활성 상태 |
| 대기 · 실패 혼합 | 적용 확인, 대기, 실패의 구분과 상세 목록 |
| 확장 기재 외부 관리 | 확장 기재를 다른 런타임이 관리하는 상태 |

**배율** 메뉴는 100%, 125%, 150%를 지원합니다. 실제 환경설정 페이지의 배율
슬라이더도 조작할 수 있습니다. **메뉴 열기**는 기본 창을 펼치며, 백틱 키는
최소화와 복원을 전환합니다. **PNG 저장** 또는 `F2`로 현재 창을 `preview.png`에 저장합니다.
샘플 전환은 기존 요청과 편집 상태를 초기화하며 현재 분류와 배율을 유지합니다.
설정 팝업이 열려 있으면 먼저 닫아야 상태를 전환할 수 있습니다.

## Linux 빌드

필요 항목은 C++23 컴파일러, CMake 3.21 이상, SDL2, SDL2_image, 한국어 글꼴입니다.
현재 저장소에서 필요한 서브모듈은 `ownership_cpp` 하나입니다.

```sh
git submodule update --init third_party/ownership_cpp
sudo apt-get install cmake ninja-build g++ libsdl2-dev libsdl2-image-dev fonts-noto-cjk
cmake -S tools/preview -B out/ui-preview -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/ui-preview --target s8_ui_preview s8_sprite_controls_test -j 4
out/ui-preview/s8_ui_preview
```

이 CMake 프로젝트는 기존 Visual Studio 게임 빌드와 독립적입니다.
저장소 전체의 프록시/DLL을 빌드하지 않습니다.

## Windows 소스 빌드

Visual Studio 2022의 C++23 지원 컴파일러와 CMake를 사용하고, vcpkg 또는 별도로
설치한 SDL2/SDL2_image CMake 패키지를 지정합니다. vcpkg 사용 예입니다.

```powershell
git submodule update --init third_party/ownership_cpp
vcpkg install sdl2:x64-windows sdl2-image:x64-windows
cmake -S tools/preview -B out/ui-preview-windows `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build out/ui-preview-windows --config Release --target s8_ui_preview
```

Linux에서 공식 SDL MinGW 개발 패키지로 교차 빌드할 때는
`cmake/mingw64.cmake`를 사용합니다. `CMAKE_PREFIX_PATH`에는 SDL2 및 SDL2_image의
`x86_64-w64-mingw32` 디렉터리 두 개를 지정합니다.

## 스크린샷과 입력 검증

```sh
sudo apt-get install xvfb
ctest --test-dir out/ui-preview -R '^sprite_controls$' --output-on-failure
python3 tools/preview/validate.py out/ui-preview/s8_ui_preview
```

`validate.py`는 `DISPLAY`가 없는 Linux에서 로컬 Xvfb를 시작하고 종료합니다.
검증 결과는 `out/ui-preview/validation.json`, 사례별 로그, `screenshots/*.png`에
저장됩니다. 이 환경에서 디스플레이 소켓이 제한되면 실행 권한이 필요합니다.

검증은 공유 ImGui 위젯의 기존 test hook에서 ID·레이블·위치를 관찰하고 **SDL
마우스/키보드/문자 입력 이벤트**로 창을 조작합니다. 별도 UI 복제본을 검사하지 않습니다.

- 8개 분류 이동, 체크박스 요청과 완료 처리
- 타이틀 상태에서 공용 무장·기재·도시 창 사용 및 주인공 편집 비활성
- 설정 준비 전 비활성, 요청 대기 중 중복 편집 방지
- 전투 설정 팝업 열기/닫기와 팝업 중 분류 이동 방지
- 실제 배율 슬라이더의 숫자 입력·완료, 125%/150%, 창 축소
- 최소화 버튼·native 접기·X·백틱으로 아이콘 전환, 클릭 복원 및 분류·위치·크기 보존
- 자동 불러오기 설정에서 아이콘으로 시작, 아이콘 밖 마우스 비점유, 작은 화면 안 위치 보정
- 연속 분류 행의 간격·크기 및 125%/150%의 슬라이더 레이블 잘림 검사
- 샘플 상태 전환, atlas 없는 ImGui 기본 버튼 동작
- helper 별도 테스트의 마우스·키보드·비활성·ID·UV·스타일 복원

단일 화면만 저장하는 예:

```sh
out/ui-preview/s8_ui_preview --scenario title --page officers --frames 8 --screenshot title.png
out/ui-preview/s8_ui_preview --scenario errors --frames 8 --screenshot errors.png
out/ui-preview/s8_ui_preview --page settings --scale 1.5 --size 1100x800
out/ui-preview/s8_ui_preview --no-atlas
out/ui-preview/s8_ui_preview --auto-load --scenario title
out/ui-preview/s8_ui_preview --minimized --frames 8 --screenshot icon.png
```

표시 장치가 없는 경우 단일 실행도 `xvfb-run -a`로 감싸거나 `validate.py`를 사용합니다.
`--font`, `--resources`로 글꼴과 리소스 경로를 명시할 수 있습니다. 리소스는 먼저
실행 파일 옆에서 찾고, 개발 빌드에서는 원본 저장소 경로를 사용합니다. PNG를
찾지 못한 버튼/프레임은 실제 UI의 기본 ImGui fallback을 사용합니다.

## 의존성과 범위

SDL backend의 버전과 소규모 API 호환 변경은 `vendor/README.md`에 기록했습니다.
Linux에서 `file_log.cpp`의 `localtime_s` 이름을 맞추는 어댑터는 이 target에만
적용합니다. 로그 writer를 실행하지 않습니다. 핵심 UI가 변경되면 독립 프리뷰를
다시 빌드하여 같은 변경을 바로 확인할 수 있습니다.
