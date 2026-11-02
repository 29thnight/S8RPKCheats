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
`start-preview.cmd`를 실행합니다. SDL DLL, `resources/ranges`, `fonts` 폴더를 함께 둡니다.
추가 콘솔 창 없이 UI 창만 열립니다.

기본 화면은 화면 비율에 맞춘 종이 패널입니다. 본문 글꼴의 기준 크기는 16px이며,
1920×1080 기준 1.2배와 사용자 UI 배율을 적용합니다. 게임과 미리보기가 같은 계산을 사용합니다.
PK 기본 편집과 겹치는 주인공·무장·명품 편집은 기본으로 숨기고, 환경설정의
**PK 중복 편집기 잠금 해제**로 표시합니다. 이 선택은 현재 실행 중에만 유지됩니다.
Windows에서 `--game-directory`로 설치된 게임 폴더를 지정하면 게임 폰트와 UI·보주 도판을 직접 읽습니다. 지정하지 않으면 기본 컨트롤을 사용합니다. 추출한 게임 UI PNG는 배포하지 않으며, 범위 선택은 별도 PNG 11장의 기존 그림 버튼으로 표시합니다.

하단의 **뒤로**는 상세 화면에서 첫 메뉴로 돌아가며 첫 메뉴에서는 창을 최소화합니다.
**−**, 우측 고정 아이콘 또는 **백틱(`)**으로도 창을 최소화할 수 있습니다.
**아이콘 클릭** 또는 **백틱(`)**으로 직전 화면을 다시 엽니다.
아이콘은 펼친 상태에도 게임 화면 우측 환경설정 아이콘 아래에 고정됩니다.
1920 × 1080 기준 지름은 **56px**이며 작은 화면에서도 클릭 영역을 최소 **48px**로
유지합니다. 편집창의 UI 배율을 바꾸거나 메뉴를 전환해도 아이콘 위치는 바뀌지 않습니다.
아이콘 우클릭에서는 설정한 알림/주인공/모든 무장 바로가기를 사용할 수 있습니다.
이미 별도로 연 편집창은 독립적으로 사용할 수 있습니다. 설정 팝업이 열려 있을 때
백틱을 누르면 팝업을 닫은 후 최소화합니다.

배포 ZIP의 `start-minimized.cmd` 또는 `--auto-load`는 실제 시작 설정 경로로
고정 아이콘부터 표시합니다. 실행 중인 게임을 사용하지 않습니다.

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

새 알림은 화면 아래 중앙의 종이 띠에 표시하고, 여러 알림은 위로 쌓습니다.
환경설정의 **알림 표시 시간**으로 노출 시간을 조정할 수 있습니다. 시간이 지나면
띠는 사라지며 내용은 알림 기록에 남습니다. 알림 띠는 게임 입력을 가로채지 않습니다.

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
- 금색 분류 버튼·상단 탭 이동, 뒤로 버튼의 상세 → 첫 메뉴 → 최소화 흐름
- 최소화 버튼·고정 아이콘·백틱으로 전환, 클릭 복원 및 직전 화면 보존
- 자동 불러오기 설정에서 아이콘으로 시작, 아이콘 밖 마우스 비점유, 작은 화면 안 위치 보정
- 제공 화면의 2048 × 1151 클라이언트 좌표에서 환경설정 아래 아이콘 정렬 검사
- UI 배율 변경·창 크기 변경·펼침 상태에서 아이콘 고정 및 분류 버튼 잘림 검사
- 125%/150%의 슬라이더 레이블 잘림 검사
- 알림 표시 시간 입력, 아래 중앙 정렬·쌓임·입력 통과·시간 만료와 기록 보존
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
`--font`, `--resources`로 글꼴과 범위 그림의 리소스 경로를 지정합니다.
범위 그림은 실행 파일 옆의 `resources/ranges`를 먼저 찾고, 개발 빌드에서는
저장소 경로를 사용합니다. 게임 UI는 Windows의 `--game-directory`로 읽습니다.

### 제공된 게임 화면에서 위치 비교

`--background`는 제공된 PNG를 정지 배경으로 표시하고 그 위에 공유 ImGui 소스를
그립니다. `--background-crop-top`은 Windows 제목 표시줄 같은 위쪽 픽셀을 제외하며,
남은 영역을 프리뷰 창 전체에 맞춥니다. 원본 PNG 파일은 변경하지 않습니다.
화면 비율을 유지하려면 `--size`를 잘라낸 게임 클라이언트의 크기와 맞춥니다.

```sh
out/ui-preview/s8_ui_preview --game-view --minimized \
  --background reference.png --background-crop-top 36 --size 2048x1151 \
  --frames 8 --screenshot launcher-reference.png
```

`--game-view`는 프리뷰 도구 모음만 숨깁니다. 이 모드도 **샘플 데이터와 정지 화면을
사용한 배치 비교**이며 실제 게임에 연결하거나 게임 UI를 조작하지 않습니다.
게임과 함께 실행한 검증 화면으로 사용하면 안 됩니다. 배경을 읽지 못하거나 위쪽
자르기가 이미지 높이 이상이면 오류로 종료합니다.

## 의존성과 범위

SDL backend의 버전과 소규모 API 호환 변경은 `vendor/README.md`에 기록했습니다.
Linux에서 `file_log.cpp`의 `localtime_s` 이름을 맞추는 어댑터는 이 target에만
적용합니다. 로그 writer를 실행하지 않습니다. 핵심 UI가 변경되면 독립 프리뷰를
다시 빌드하여 같은 변경을 바로 확인할 수 있습니다.

## Windows 게임 원본으로 확인

`--game-directory`를 지정하면 설치 폴더의 글꼴과 UI 텍스처를 읽습니다.
게임 프로세스와 세이브는 사용하지 않고 표시값과 명령 결과는 샘플 세션에서 처리합니다.
원본 글꼴이나 UI 리소스를 읽지 못하면 이 확인 실행은 실패합니다.

```powershell
out/ui-preview-windows/Debug/s8_ui_preview.exe --game-directory "C:/Program Files (x86)/Steam/steamapps/common/SAN8R" --open jewels --game-view --size 1600x900
```

- `--open jewels`: 계통별 보주 도판. 미개방 → 노란 개방 → 빨간 보조계통 강제 적용 → 미개방 순으로 클릭한다.
- `--unlock-pk`: 숨긴 자체 편집기를 처음부터 표시한다. 기본값은 숨김이다.
- `tests/jewel-cycle.txt`: 세 번 클릭 후 개방 비트와 강제 적용 비트를 확인하고 긴 알림 두 개를 표시한다.
- `tests/pk-editors.txt`: 기본 숨김 → 잠금 해제 → 무장 편집 열기 → 재잠금 후 숨김을 확인한다.

2026-11-01 확인 결과: 보주 클릭과 긴 알림은 1280×720·1600×900에서 통과,
개요는 1920×1080에서 표시 확인. PK 잠금 전환 검사도 통과했다.
샘플 명령 결과를 확인한 것이며 게임 메모리에 쓰는 동작은 실행하지 않았다.

## PK 편집 범위를 정한 근거

[공식 매뉴얼의 편집 설명](https://www.gamecity.ne.jp/manual/sangokushi8-re/wpk/jp/3600.html)은
주인공의 금·담력, 무장의 능력·기재·공적, 도시의 금·군량, 세력 관계, 명품의 능력·가치 편집을 안내한다.
[공식 PK 기능 소개](https://www.gamecity.ne.jp/sangokushi8-re/wpk/new-features/)는 도시 내정 수치와
NPC 친밀 자동 상승 설정도 명시한다. 이 범위와 제공된 게임 편집 화면을 대조해 중복 수동 편집을 숨겼다.
도시의 금·군량·병사는 기본적으로 조회만 표시하고, 자동 수송·한도 변경·전체 일괄 작업은 남긴다.
기재 설명 수정·무작위 배분·추가 전투 규칙처럼 기본 편집과 다른 작업도 남긴다.
잠금 해제는 자체 편집기를 표시하며 게임의 편집 메뉴를 자동으로 열거나 시나리오의 설정을 바꾸지 않는다.

보주 도판은 `HojuStill_PK1KTP`의 11개 G1T, 원형 장식은 `ac311b9c`에서 읽는다.
`src/domain/jewel_layout.h`는 `SleLayout`의 `035_01_02_81`부터 `91`까지 KSCL에서 추출한
버튼 중심과 연결선이다. 버튼 번호 순서와 `SECONDARY_JEWEL_DEFINITIONS`를 대응했다.
