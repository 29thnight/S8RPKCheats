# README 작성 준비

기준: 2026-11-01 로컬 작업본. 현재 README를 교체하기 전의 구성안과 근거 목록이다.

## 독자와 구성

첫 화면은 게임 사용자를 대상으로 설치와 조작 방법을 설명한다. 개발 환경, 구조, 조사 기록은 뒤에 둔다.

최종 README의 구성:

1. 프로젝트 소개: 삼국지 8 REMAKE with 파워업키트용 C++23 치트·편집 오버레이.
2. 설치: 빌드한 `hid.dll`을 `SAN8RPK.exe`와 같은 폴더에 복사.
3. 기본 조작: 백틱 키로 창 열기·최소화, 화면 아이콘으로 다시 열기.
4. 기능: 실제 메뉴에 연결된 기능을 분류별로 설명.
5. 기재 효과 편집과 `VERSION.dll` 연동.
6. 설정 파일과 게임 리소스 사용.
7. 소스 빌드 및 독립 UI 미리보기.
8. 동작 확인 현황과 남은 작업.
9. 출처·의존성·개발 문서.

## 본문에 사용할 확인 사항

| 항목 | 확인한 내용 | 근거 |
|---|---|---|
| 저장소 | `https://github.com/29thnight/S8RPKCheats.git` | Git origin |
| 클론 | `git clone --recurse-submodules https://github.com/29thnight/S8RPKCheats.git` | [서브모듈](../../.gitmodules) |
| 솔루션 | `S8RPKCheats2026.sln` | [현재 README](../../README.md) |
| 도구 | VS 2026, C++ 도구 v145, MSVC 14.51.36231, Windows SDK 10.0.26100.0 | [프록시 프로젝트](../../src/proxy/S8.Hid.vcxproj) |
| 언어 설정 | C++23 preview, UTF-8, NOMINMAX, vcpkg 자동 링크 비활성화 | [속성 시트](../../build/S8.Cpp23.props) |
| 출력 | `out/x64/Debug/hid.dll`, Release 구성의 출력 경로도 별도 제공 | [프록시 프로젝트](../../src/proxy/S8.Hid.vcxproj) |
| 조작 키 | 백틱 키 `VK_OEM_3` | [입력 처리](../../src/overlay/overlay_present.cpp) |
| 주 설정 | DLL과 같은 폴더의 `S8RPK_cheat_config.json` | [설정 경로](../../src/platform/windows/config_file.cpp) |
| 기재 효과 | 기재별 효과 6개의 종류·값·P 편집, 카탈로그 반영, JSON 저장 | [편집 UI](../../src/overlay/trait_text_editor.cpp), [편집 데이터](../../src/domain/trait_text_edit.cpp), [JSON 저장](../../src/services/trait_text_config.cpp) |
| 외부 기재 적용 | 파일 저장 후 같은 프로세스의 `SAN8R_EditorGUI`에 설정 로드 명령 전달. DLL 경로와 확인된 창 처리 함수 위치를 검사 | [저장 서비스](../../src/services/trait_text_service.cpp) |
| 외부 적용 상태 | 명령 전달 완료와 실제 게임 효과 확인은 구분. 외부 기재 적용의 게임 테스트는 아직 미실행 | 직전 빌드·복사 결과 및 저장 서비스 |
| 효과 목록 | 외부 `effect_definitions.json`이 없으면 DLL 내장 리소스 사용 | [초기 로딩](../../src/runtime/runtime_services.cpp), [내장 리소스](../../src/proxy/resources.rc) |
| 미리보기 | 실제 편집 UI를 공유하며 샘플 데이터 사용 | [미리보기 안내](../../tools/preview/README.md) |
| 외부 의존성 | MinHook, Dear ImGui, JSON for Modern C++, stdexec, 리소스 해석 코드 등 | [출처·고지](../../third_party/README.md) |

## 설치 설명에서 구분할 파일

- 실행 DLL: `hid.dll`.
- 사용자 설정: `S8RPK_cheat_config.json`.
- 외부 기재 설정: `san8r_traits_config.json`. 외부 DLL이 없을 때 기본 카탈로그 후보는 `S8RPK_traits_default.json`.
- 기본 기재 텍스트 설정: `trait_texts.ini`.
- 외부 종합편집기 설정: `san8r_f8_config.json`. 우리 UI의 종합편집기 전체 탭 연동은 남아 있다.
- 폰트·UI 텍스처: 게임 리소스 읽기 경로와 DLL 내장 리소스를 구분해서 설명한다. 개발 PC의 Downloads 경로를 설치 필수 경로로 쓰지 않는다.
- 선택 파일 전체 목록은 초기 로딩 코드와 대조한 뒤 확정한다. 파일이 없을 때의 내장 데이터·기본값 사용 여부를 각각 기재한다.

## 기능 설명의 기준

[현재 기능 대응](Implementation_Parity.md)과 실제 메뉴를 대조한다. 오래된 조사 문서의 상태를 그대로 현재 상태로 옮기지 않는다.

- 메뉴와 명령 경로가 구현된 항목: 구현된 조작을 구체적으로 설명.
- 독립 미리보기로 확인한 항목: 샘플 UI 확인으로 표시.
- 실제 게임에서 확인한 항목: 확인한 게임 상태와 조작만 기재.
- 외부 DLL 소유 기능: 사용하는 설정 파일과 적용 요청 경로를 명시.
- 남은 항목: 종합편집기 탭별 연동, 보조 보주 후크 충돌에 대한 연동 처리, 최신 기재 효과 편집의 실제 게임 동작 확인.

## 빌드 설명 초안

Visual Studio 2026에서 솔루션을 열고 `Debug | x64` 구성의 HID 프록시 프로젝트를 빌드한다.

Developer PowerShell에서는 다음 명령을 사용한다.

```powershell
msbuild src/proxy/S8.Hid.vcxproj /m /p:Configuration=Debug /p:Platform=x64
```

직전 작업에서 Debug x64 빌드와 게임 폴더 복사가 완료됐다. 빌드에는 경고가 있었다. Release 빌드 성공이나 최신 변경의 게임 동작 성공으로 확대해서 적지 않는다.

테스트 프로젝트는 Debug x64 구성만 제공한다. 미리보기 빌드는 [별도 안내](../../tools/preview/README.md)를 연결한다.

## 본문 작성 전에 마무리할 확인

- 배포에 필요한 선택 파일 목록과 기본값·내장 리소스의 대응.
- 지원 게임 버전: 현재 코드의 주소·서명 기준과 실제 확인한 게임 버전을 대조. 모든 PK 버전을 지원한다고 적지 않는다.
- 대표 스크린샷: 현재 작업본의 화면을 사용하고, 원본 게임 화면·미리보기·실제 치트 화면을 구분.
- 설치 과정: 기존 `hid.dll`이 있는 경우의 교체 안내와 제거 방법.
- 루트 라이선스 파일 및 원저작자 고지 상태. 문서 작성만으로 라이선스나 저자 정보를 변경하지 않는다.
- [외부 의존성 안내](../../third_party/README.md)의 과거 “미빌드” 문구와 현재 빌드 사실을 구분해 갱신.

현재 README의 설계·P0·기능 대응·출처 링크는 개발 문서 항목에 보존한다.
