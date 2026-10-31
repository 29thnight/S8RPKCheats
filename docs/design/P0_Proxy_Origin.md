# P0 프록시 ABI와 실행 골격 출처

S8RPKCheats `068ca1d881edf870f7cf06f935cf50e286004af8`과 NightFyre `b44d74cbb20918c1b616190325ee49589b80cf36`의 정적 소스를 비교했다. 두 저장소는 `reference/` 아래 독립 클론이다. 비교는 아래 접점의 유사점과 차이에 한정하며 저장소 전체의 저작 경계를 확정하지 않는다.

## 프록시 구성

[소스에 선언된 export 목록](p0/proxy_exports.csv)은 링커 pragma와 dxgi.def의 이름·전달 대상이다. 컴파일된 DLL의 export table을 검사한 결과는 아니다.

| 종류 | 소스 선언 | 초기화 경로 | 이관 기준 |
|---|---|---|---|
| hid | `dllmain_hid.cpp`의 시스템 hid로 전달하는 40개 pragma export | process attach에서 `InitializeOnce`로 MainThread 생성 | 현재 기본 `ProxyType=hid`. 이름, 포워딩 대상, 실제 호출 가능 여부 검증. |
| dinput8 | `dllmain.cpp`의 시스템 dinput8로 전달하는 6개 pragma export | process attach에서 MainThread 생성 | 시스템 DLL과 export 이름·호출 규약 검증. |
| dxgi | `dllmain_dxgi.cpp`의 13개 pragma export 및 `dxgi.def`의 factory 3개 매핑 | attach에서는 모듈만 기록. 세 factory wrapper 호출 시 시스템 DLL을 로드하고 MainThread 시작. 포인터가 없으면 `E_FAIL`. | factory 호출과 시작 순서, 원본 DLL 참조 수명, 3개 factory의 ABI 검증. 프로젝트의 def 지정은 Release x64와 `ProxyType=dxgi`에만 명시된다. |

포워더의 경로는 pragma에 `C:\Windows\System32`로 명시되어 있고 dxgi factory wrapper는 `GetSystemDirectoryA`를 사용한다. 현재 스크립트는 플랫폼을 명시하지 않으므로 실제 생성 결과는 평가·빌드 후 확인해야 한다. 빌드는 실행하지 않았다.

## 기준 소스 비교

| 접점 | NightFyre 기준 | 현재 S8RPKCheats | 이관 판단 |
|---|---|---|---|
| 실행 루프 | `Source.cpp`의 MainThread가 Engine을 만들고 D3D hook을 설치하며 `ClientBGThread`와 각각 1ms sleep/yield loop를 수행한다. | `Source_impl.inc:227,280`은 같은 이름과 구조의 루프를 사용하되 각각 30ms 대기, 게임 초기화 재시도와 개별 종료 처리를 추가한다. | 루프 골격은 교체하되 조기 책략 준비·설정 적용·기능 worker 종료 순서는 보존한다. |
| 렌더 초기화 | `src/Engine.cpp:242,262`에서 임시 DX11 장치의 vtable을 복사하고 Present와 ResizeBuffers를 hook한다. | `Engine.cpp:234,257`은 같은 임시 장치·vtable 방식이나 Present만 설치한다. | 실제 현재 경로를 기준으로 하며 resize는 독립 검증한다. |
| 입력 | NightFyre `src/Engine.cpp:192`는 WndProc에서 메뉴 상태에 따라 ImGui 처리 또는 원래 WndProc로 전달한다. | `Engine.cpp:89`에 마우스·키보드 캡처와 IME 처리 경로가 추가되었다. | 한글 IME, 포커스, 입력 캡처와 DPI를 보존한다. |
| 소유·종료 | NightFyre `src/Engine.cpp`의 Engine은 Console·D3D11Window·Hooking을 전역 소유하고 원본 코드에 `release()`가 있다. | `Engine.cpp`의 RenderManager와 Hooking, `Source_impl.inc`의 명시적 정상·프로세스 종료 분기가 있다. | 모듈별 자원과 callback의 종료 조건을 따로 확정한다. |
| 외부 라이브러리 | NightFyre README는 Dear ImGui와 MinHook을 외부 라이브러리로 명시한다. | 자체 프로젝트에도 Framework/ImGui와 Hooking/MinHook·HDE가 컴파일 입력에 포함된다. | 골격 코드와 외부 라이브러리의 출처를 따로 기록한다. |

`Hooking/hookmain.h`는 Kiero 1.2.6으로 표시하며 D3D11과 MinHook을 활성화한다. `hookmain.cpp`는 프로젝트의 컴파일 항목이다. 자체 소스 전체에서 `kiero::` 호출 또는 해당 헤더 include를 검색했을 때 Hooking 폴더 외 직접 참조는 찾지 못했다. 이것은 링크 후 제거 여부나 외부에서의 참조 부재를 증명하지 않으므로 삭제 근거로 쓰지 않는다.

## 고지의 현재 상태

- NightFyre README에는 ImGui·MinHook 출처 링크가 있고, 해당 체크아웃에서 별도 LICENSE/NOTICE 파일은 검색되지 않았다. NightFyre 자체 구현의 재배포 조건은 확인되지 않았다.
- 현재 `MinHook.h`에는 저작권 및 배포 조건이, HDE 소스에는 Vyacheslav Patkov 저작권이 표시되어 있다. 저장소의 `imstb_*` 헤더 끝에는 MIT 또는 public-domain 선택 조항이 있다.
- 현재 ImGui의 WIP 버전과 Kiero 헤더 버전 표시는 정확한 상류 커밋 또는 로컬 수정분을 식별하지 않는다. 원문 고지와 실제 포함 파일 버전을 확인한 후 별도 의존성 목록을 고정한다.

## NightFyre 포함 라이브러리 파일 비교

기준 NightFyre `b44d74cbb20918c1b616190325ee49589b80cf36`의 `libs/ImGui`, `libs/MinHook`과 현재 S8RPKCheats의 `Framework`, `Hooking`을 같은 파일명으로 비교했다. [파일별 결과](p0/nightfyre_library_comparison.csv)는 UTF-8 BOM과 CRLF 차이를 별도로 분류한다. 상류 공식 배포본과의 비교가 아니라 두 기준 저장소 사이 비교다.

- ImGui 공통 15개 중 12개는 BOM·줄바꿈을 정규화한 후에도 내용이 다르고, 3개는 BOM 또는 줄바꿈만 다르다. 현재 저장소에는 NightFyre에 없는 DX12 backend cpp/h 두 개가 추가되어 있다. 표시 버전은 NightFyre `1.90.6`, 현재 `1.91.0 WIP`다. 이 결과만으로 차이 전체가 로컬 수정이라고 판단할 수 없다.
- Hooking 공통 18개 중 5개는 바이트까지 동일하고 11개는 BOM 또는 줄바꿈만 다르며, 2개는 정규화 후에도 차이가 있다. 동일한 5개는 `hde32.c`, `hde64.c`, `hook.c`, `minhook.def`, `trampoline.c`다. `hookmain.cpp`의 확인된 차이는 PCH 상대 경로와 빈 줄·끝 줄바꿈이다. 두 기준 `hookmain.h` 모두 Kiero `1.2.6`으로 표시한다.
- S8RPKCheats의 `Framework/imgui.cpp`, `hook.c`, `hookmain.cpp` 각각의 마지막 변경은 저장소 이력에서 최초 커밋 `3b66385589b66053164fbca40d6aeb7452c26899`이다. 이는 그 커밋 이전의 상류 원본을 식별하지 않는다.

정확한 ImGui WIP 상류 커밋, Kiero·MinHook 배포본과의 변경분, 각 원문 라이선스 전체 및 폰트·이미지 출처는 별도 확인이 남아 있다. 파일명과 버전 문자열만으로 원본 라이선스 전체를 확정하거나 현재 차이를 삭제하지 않는다.

## Hooking 디렉터리의 파일별 고지

기준 파일의 머리말을 확인했다. `buffer.c/.h`, `hook.c`, `trampoline.c/.h`, `MinHook.h`는 Tsuda Kageyu의 2009–2017 저작권 및 소스·바이너리 재배포 시 고지 유지 조건을 포함한다. `pstdint.h`에도 같은 저작권과 두 항목의 조건이 있으나 면책 문구 표현은 별도로 존재하므로 파일 자체 고지를 보존해 확인한다.

`hde32.c/.h`, `hde64.c/.h`, `table32.h`, `table64.h`에는 Vyacheslav Patkov 저작권이 표시되며, 확인한 HDE 파일 머리말에는 별도의 재배포 조건 전문이 없다. `hookmain.h`는 Kiero 1.2.6과 DX11·MinHook 활성 설정을 표시하지만 머리말에 별도 권리 고지는 없다. 이 파일의 표시만으로 HDE나 Kiero의 전체 배포 조건을 확정하지 않는다.

프로젝트는 `buffer.c`, `hde32.c`, `hde64.c`, `hook.c`, `trampoline.c`를 컴파일 항목으로 선언하며 HDE 구현은 각각 아키텍처 매크로로 분기한다. 원문 고지 및 정확한 상류 revision 확인과 배포물에 필요한 고지 정리는 후속 과제로 남는다.

## ImGui와 내장 stb의 파일별 표기

`Framework/imgui.cpp:28`은 Omar Cornut의 2014–2024 저작권과 별도 `LICENSE.txt` 참조를 표시하고, `imgui.h`의 문자열은 `1.91.0 WIP`, 숫자 매크로는 `19093`이다. 문자열과 숫자가 가리키는 것은 소스에 기록된 버전 표기이며 정확한 상류 커밋은 아니다. 해당 파일의 변경 이력 설명에는 2024-07-02 항목이 포함되고 기준 저장소 최초 커밋은 2026-04-03이다. 이 날짜만으로 상류 파일이 그 기간 중 어느 시점에 복사됐는지는 확정할 수 없다.

`imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`는 각각 1.01, 1.14, 1.26으로 표기되어 있으며 각 파일 끝에 Sean Barrett의 2017년 저작권을 포함한 MIT 선택지와 public-domain 선택지를 함께 담고 있다. 이 고지는 세 헤더 자체의 내용이며 Dear ImGui 전체나 자체 게임 코드에 자동으로 적용되는 것으로 취급하지 않는다. 전체 ImGui 원문 라이선스와 정확한 원본 revision·변경분은 별도 확인이 남는다.

## 프록시 초기화 실패와 동시 호출

세 프록시의 `InitializeOnce`는 일반 정적 bool을 먼저 true로 바꾼 뒤 `CreateThread`를 호출한다. 스레드 생성이 실패해도 flag를 되돌리거나 재시도하지 않는다. hid·dinput8은 attach에서 호출하고 dxgi는 각 factory wrapper에서 호출한다. dxgi의 factory들이 여러 스레드에서 동시에 호출되는 경우 이 bool 접근을 보호하는 동기화는 함수에 없다.

`dllmain_dxgi.cpp`의 `LoadOriginal`은 전역 모듈 핸들이 없을 때 시스템 DLL을 로드하고 세 함수 포인터를 순서대로 가져온다. 모듈 로드 실패는 이후 호출에서 다시 시도할 수 있지만, 모듈 로드는 성공하고 특정 `GetProcAddress`가 실패한 경우 다음 호출은 모듈 핸들이 있다는 이유로 다시 조회하지 않는다. 해당 wrapper는 포인터가 없으면 `E_FAIL`을 반환한다. 로드와 포인터 게시에도 공통 잠금이 없으므로 동시 factory 호출에서의 순서는 실제 검증 대상이다. 확인된 코드 경로와 별개로 DLL export 구성은 [빌드 입력 조사](P0_Build_Inputs.md)의 구성별 def 조건에 좌우된다.

세 DXGI factory wrapper는 모두 `LoadOriginal` 다음에 `InitializeOnce`를 호출하고 나서 함수 포인터를 검사한다. 따라서 시스템 DLL 로드나 해당 export 조회가 실패해 wrapper가 `E_FAIL`을 반환하는 경우에도 초기화 스레드 생성은 시도된다. 반대로 스레드 생성에 실패해도 유효한 원본 factory 포인터가 있으면 원본 호출과 그 반환값은 그대로 진행된다. 원본 factory의 성공과 자체 초기화의 성공은 서로 다른 상태로 기록하고 이관 시 각각 검증한다.

회귀 조건은 스레드 생성 실패, 원본 DLL 로드 실패와 재시도, 세 export 중 일부 조회 실패, factory 동시 호출과 프로세스 종료다. 프록시를 빌드하거나 호출하지 않았다.

## 공식 상류 기준과 Kiero 1.2.6 비교

공식 저장소의 이력을 2026-10-31에 조회해 표시 버전과 가까운 기준을 대조했다. Kiero 공식 이력에서 1.2.6을 도입한 커밋은 `a7687569e19a773a3bf48874f434bcfc14e386a2`, 다음 버전 1.2.7 커밋의 직전 상태는 `358d2cdab40465f9fe97a4034063416e54b9d7a5`다. 두 상태 사이 `kiero.cpp`에는 2019-07-09 전처리·스타일 변경이 있다.

후자 상태의 `kiero.h`를 기준 저장소 `hookmain.h`와 줄 단위로 대조하면 BOM 외에는 D3D11과 MinHook 설정을 각각 0에서 1로 바꾼 차이가 확인된다. `kiero.cpp`와 `hookmain.cpp`는 PCH 추가, 헤더·MinHook include 경로, 빈 줄과 끝 줄바꿈에 차이가 있다. 1.2.6 최초 커밋의 cpp에는 이 밖에 `#ifdef` 대 `#if`, `memcpy` 표기 등 차이가 있으므로 버전 번호만으로 동일 파일이라 판단할 수 없다. 이 비교는 가까운 공개 이력의 파일 내용에 대한 것이며, 실제 복사 경로나 정확한 출처 커밋을 증명하지 않는다.

대조 대상으로 공식 ImGui `v1.91.0` (`8199457a7d9e453f8d3d9cadc14683fb54a858b5`)의 `imgui.cpp`와 `imgui.h`를 살펴보면 현재 파일과 내용이 다르다. ImGui는 현재 파일이 WIP로 표기되어 있으므로 정식 태그와 다르다는 사실만으로 로컬 수정이라고 결론 내릴 수 없다. 공식 MinHook `v1.3.3` (`9fbd087432700d73fc571118d6a9697a36443d88`)의 `hook.c`와 `MinHook.h`도 현재 파일과 바이트 및 텍스트가 다르며, 살펴본 차이에는 include 경로와 공백 형식이 포함된다. 전체 의미 차이와 정확한 상류 기준은 확인하지 않았다.

공식 저장소: [Kiero](https://github.com/Rebzzel/kiero), [Dear ImGui](https://github.com/ocornut/imgui), [MinHook](https://github.com/TsudaKageyu/minhook). 비교는 소스 읽기만 수행했다.

### MinHook v1.3.3 파일 전체 대조

공식 v1.3.3 커밋 `9fbd087432700d73fc571118d6a9697a36443d88`의 동일 이름 파일 14개와 현재 `Hooking`을 대조했다. [파일별 경로·SHA-256·결과](p0/minhook_v133_comparison.csv)에서 `table32.h`, `table64.h`, `minhook.def` 세 파일은 바이트까지 같고, 나머지 11개는 UTF-8 BOM과 CRLF를 정규화해도 다르다. [정규화된 전체 텍스트 차이](p0/minhook_v133_changes.diff)를 함께 보존했다. 예를 들어 `buffer.c`에는 포인터 표기·주석 문자 차이가 있고 HDE 구현에도 더 많은 차이가 있다. 이 목록은 차이를 찾기 위한 증거이며 의미가 모두 같거나 다르다는 판정이 아니다. v1.3.3이 실제 도입 원본이라는 뜻도 아니다. 컴파일과 실행은 하지 않았다.

같은 14개를 주석과 공백을 제외한 문자열·식별자·숫자·구두점 토큰 순서로도 비교했다. [토큰 비교표](p0/minhook_v133_token_comparison.csv)에서 12개는 순서가 같고 `hook.c`와 `trampoline.c`의 차이는 각각 MinHook 헤더와 HDE 헤더 두 개의 include 경로다. HDE의 많은 텍스트 차이는 이 비교에서 주석·공백 차이로 분류된다. 단순 토큰 비교는 줄 경계와 전처리 결과를 평가하지 않으므로 조건부 컴파일이나 실제 동작의 동등성을 증명하지 않는다. 원래 고지와 텍스트는 위 전체 diff를 기준으로 별도 보존한다.

줄 이어쓰기를 합친 뒤 줄 시작 `#` 지시문의 공백을 제거해 별도로 대조한 [지시문 비교표](p0/minhook_v133_directive_comparison.csv)에서도 12개는 순서가 같고 나머지 두 파일의 차이는 위 세 include 경로다. 이 검색은 텍스트상 지시문 후보의 비교이며 블록 주석 내부 여부, 컴파일러 기본 매크로, include 해석과 실제 전처리 결과는 평가하지 않는다. 실제 소스의 지시문 배치와 조건별 활성 여부는 여전히 빌드 구성에 따라 확인해야 한다.

## ImGui WIP 공개 이력과 파일별 비교

공식 저장소 기본 브랜치에서 2024-06-01부터 2024-07-31까지 `imgui.h`를 변경한 커밋 127개의 파일을 가져와 기존 `Framework/imgui.h`와 줄 단위로 비교했다. 확인한 후보 중 가장 가까운 것은 `0ebf49b4c1fa9256a2ea002245a1186075fe7567` (2024-07-02)이다. 이는 검색 기간과 기본 브랜치의 후보에 한정한 유사도 결과이지 복사 원본 커밋을 증명하지 않는다.

이 후보 커밋의 대응 파일과 현재 `Framework`의 17개 파일을 비교한 결과는 [파일별 해시와 비교표](p0/imgui_upstream_comparison.csv)에 기록했다. 바이트가 같은 파일은 없고, UTF-8 BOM과 CRLF만 정규화하면 9개가 같다. 나머지 8개는 정규화 후에도 다르다. 파일마다 다른 상류 시점이나 다른 변경이 섞였을 수 있으므로 모든 차이를 자체 수정으로 단정하지 않는다.

해당 후보의 `imgui.h`와의 내용 차이는 `SmallCheckbox`, `CustomSliderInt`, `CustomSliderScalar` 선언 세 개와 `ImGuiIO::ImeWindowHandle` 필드 추가다. 대응 구현 세 함수는 `imgui_widgets.cpp`에 있다. 자체 cpp/h/inc의 텍스트 검색에서 세 함수의 라이브러리 외 호출은 찾지 못했다. `Engine.cpp:549`는 추가된 `ImeWindowHandle`에 게임 창 핸들을 기록하지만, `Framework` 안에서 이 필드의 활성 참조는 선언 외에 찾지 못했다. 이 결과만으로 IME 전체가 작동하지 않는다고 결론 내리지 않으며, 실제 IME 처리는 별도로 WndProc·Win32 backend 경로를 확인해야 한다. 추가 필드는 `ImGuiIO`의 구조 배치를 바꾸므로 상류 교체 시 관련 컴파일 단위의 버전을 함께 맞춰야 한다.

이 파일 비교는 소스 확인이며 빌드·실행 결과가 아니다. 후보 외 브랜치와 기간, 8개 차이 파일의 전체 의미 차이 및 실제 도입 경위는 미확인이다.

## 현재 PC의 x64 시스템 DLL export 이름 대조

빌드하지 않고 Visual Studio `dumpbin /exports`로 이 PC `System32`의 세 DLL을 읽고, 소스 pragma 및 `dxgi.def`의 이름 색인과 비교했다. 대상 버전은 hid·dinput8 `10.0.26100.9949`, dxgi `10.0.26100.9818`이다. [이름·시스템 ordinal 비교표](p0/system_proxy_export_comparison.csv)는 이 PC의 시스템 DLL과 소스 선언의 비교이며 새 DLL의 export table이 아니다. PowerShell의 기본 대소문자 비구분 이름 집합으로 비교했으므로 대소문자까지 동일한 ABI를 증명하지 않는다.

| 대상 | 양쪽에 이름 있음 | 소스 선언만 있음 | 이 PC 시스템 DLL에만 있음 |
|---|---:|---:|---:|
| hid | 34 | 6 | 13 |
| dinput8 | 6 | 0 | 0 |
| dxgi | 12 | 4 | 8 |

소스에만 있는 hid 이름은 `HidP_GetButtons`, `HidP_GetButtonsEx`, `HidP_GetUsageAndPageList`, `HidP_SetButtons`, `HidP_TranslateUsageAndPagesToI8042ScanCodes`, `HidP_UnsetButtons`다. dxgi는 `ApplyCompatResolution`, `DXGIGetDebugInterface`, `SetAppCompatString`, `SetAppCompatValue`다. 이 이름들을 전달 대상으로 사용하는 경로가 이 시스템에서 호출된다면 대응하는 이름을 찾지 못한다. 시스템에만 있는 이름 역시 현재 소스 목록만 보면 대응 export 선언이 없다. 이름이 비슷한 다른 항목을 자동으로 같은 기능으로 취급하지 않는다.

대조한 파일의 SHA-256은 hid `E8043B16F4D7FD2B815FFD1638D2DEF7BE4C0E0E324FE2DCEE3B262BDCE0D2AC`, dinput8 `B0F10001FCCD05E9863D84588BE9B9F8389656D95177287DD4C6CF3BE4BF1DFE`, dxgi `84BC502F36687BB6C4DD9763D109C6ED7339767ED50A8A70707761563DD3E816`다. 결과는 이 시스템 버전의 이름 비교에 한정한다. ordinal 호환성, 실제 프록시의 export·전달 결과 및 다른 Windows 버전은 확인하지 않았다. dxgi factory 세 이름은 소스의 def 매핑이 Release x64 dxgi 구성에 지정된 조건과 함께 판단한다.

## 설치된 게임의 정적 import와 기존 hid DLL

이 PC의 `SAN8R/SAN8RPK.exe`를 실행하지 않고 PE import table을 읽었다. 확인한 파일은 x64, 57,585,488바이트, SHA-256 `000D450345728C23BEBCA581AB37B8BCF25266172767B5E6DEFB10A72A8C9457`이며 같은 폴더 `UpdateVer.txt`는 `1.1.3`이다. 일반 import table은 `DINPUT8.dll`의 `DirectInput8Create`와 `dxgi.dll`의 `CreateDXGIFactory`를 이름으로 요구한다. 이 두 이름은 위 소스 선언과 현재 시스템 DLL에 있다. 해당 실행 파일의 일반 import 목록에는 `hid.dll`이 없다. 이것은 다른 모듈의 의존성, 지연 로드, 동적 `LoadLibrary`/`GetProcAddress` 또는 실제 로더 선택까지 배제하지 않는다.

같은 게임 폴더에 이미 존재하던 `hid.dll`도 변경하거나 실행하지 않고 export table을 읽었다. 이 파일은 x64, 2,117,120바이트, SHA-256 `A3A6EBCDE1774EA7CA2295295B069B9096EA5F61A689B2DB2AD0D545C7539105`다. 40개 export 이름은 기준 소스의 hid 선언 40개와 대소문자를 구분해 일치하며 모두 시스템 hid에 대한 forwarder로 기록돼 있다. [기존 DLL 이름·ordinal 표](p0/installed_hid_exports.csv)는 현재 시스템 DLL의 같은 이름 및 ordinal도 함께 기록한다. 이름이 공통인 34개 중 11개는 두 DLL의 ordinal이 다르다. ordinal만으로 import하는 호출자는 이름 비교만으로 호환성을 판단할 수 없다. 소스에서 빌드한 결과가 이 파일과 같거나 이 파일이 기준 커밋에서 생성됐다는 뜻은 아니다.

`System32`의 dinput8·d3d11·dxgi·xinput1_4 네 DLL에서 일반 import로 hid를 직접 참조하는 항목은 찾지 못했다. 이 네 파일만의 확인이며 로드 경로 전체의 조사 결과는 아니다. 실제 게임 또는 DLL을 실행하지 않았다.

### 정적 의존성 후보의 제한된 탐색

게임 실행 파일에서 시작해 `dumpbin /dependents`가 표시한 DLL 이름을 세 단계까지 따라갔다. 각 단계에서 같은 이름이 있으면 게임 폴더, 없으면 `System32` 파일을 후보로 삼았고 `api-ms-*`·`ext-ms-*`는 해석하지 않았다. 해당 규칙으로 선택된 PE 파일 118개를 조사한 결과, 표시된 의존성 5,401개 중 `hid.dll`이라는 이름은 없었다. 탐색한 연결과 선택 후보는 [정적 의존성 후보 목록](p0/game_import_graph_candidates.csv)에 기록했다. 4,438개는 API set 이름이라 미해석이고, `HvsiFileTrust.dll`과 `wpaxholder.dll` 두 항목은 두 탐색 위치에서 찾지 못했다.

이 목록은 두 폴더의 파일을 정해진 규칙으로 선택한 **후보 그래프**다. Windows의 KnownDLL, API set, side-by-side, 사용자별 로더 설정, 실제 경로 또는 동적 로드와 모든 지연 로드 실행을 재현하지 않으며 조사 깊이도 제한돼 있다. 따라서 이 결과로 설치된 `hid.dll`이 실제 로드되지 않는다고 판단하지 않는다. 기존 DLL과 게임은 실행하지 않았다.

### 기준 저장소의 별도 라이선스 파일 확인

2026-10-31 기준 두 참조 저장소에서 Git 추적 파일을 `LICENSE`, `license`, `COPYING`, `NOTICE` 이름 패턴으로 조회한 결과 일치하는 경로가 없었다. `Framework` 폴더에도 별도 license 이름 파일은 보이지 않는다. `imgui.cpp`는 표준 MIT 라이선스의 `LICENSE.txt`를 참조하지만 그 참조 파일은 이 기준 트리에서 확인되지 않았다. 반면 앞 절처럼 MinHook과 stb 일부는 소스 파일 안에 고지가 있다.

이 조회는 위 이름 패턴과 기준 커밋에 한정하며, 다른 이름의 고지·외부 배포물·자산별 권리나 정확한 상류 revision까지 확인한 결과는 아니다. 이관 시 실제로 포함할 구성 요소별 원문 고지와 출처를 따로 확보·대조해야 한다. 파일 변경이나 빌드는 하지 않았다.

### 공식 비교 기준의 고지 원문

앞에서 파일 내용을 비교한 **후보 커밋**의 공식 고지를 직접 확인했다. [Dear ImGui 후보의 MIT 고지](https://github.com/ocornut/imgui/blob/0ebf49b4c1fa9256a2ea002245a1186075fe7567/LICENSE.txt)는 2014–2024 Omar Cornut 저작권을, [Kiero 비교 기준의 MIT 고지](https://github.com/Rebzzel/kiero/blob/358d2cdab40465f9fe97a4034063416e54b9d7a5/LICENSE)는 2014–2020 Rebzzel 저작권을 표시하며 각각 저작권·허가 고지의 포함 조건을 담는다.

[MinHook v1.3.3 비교 기준의 고지](https://github.com/TsudaKageyu/minhook/blob/9fbd087432700d73fc571118d6a9697a36443d88/LICENSE.txt)는 MinHook과 HDE 32·64의 각 저작권 및 소스·바이너리 재배포 조건을 함께 싣고 있다. 기준 저장소 HDE 개별 머리말에 조건 전문이 없다는 앞선 관찰을, 공식 비교 기준에는 HDE 조건 전문이 있다는 근거로 보완한다.

이 링크들은 기존 파일과 유사한 공식 비교 후보의 원문이다. 기존 파일의 정확한 도입 revision을 확정하거나, 다른 자산·자체 코드의 출처를 확인한 것은 아니다. 사용자 저장소에는 아직 고지를 추가하지 않았고 실제 포함·배포 범위는 후속 이관에서 결정한다.

### 컴파일 입력과 고지 대조표

다음 표는 x64 구성 평가 및 기준 트리의 연결을 정리한 것이다. 파일 비교 후보와 실제 도입 출처는 구분한다.

| 구성 요소 | 기준 소스의 위치·입력 | 고지 근거 | 남은 확인 |
| --- | --- | --- | --- |
| Dear ImGui core·demo | `Framework/imgui.cpp`, `imgui_demo.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` | 각 파일 표기와 [공식 후보 MIT 원문](https://github.com/ocornut/imgui/blob/0ebf49b4c1fa9256a2ea002245a1186075fe7567/LICENSE.txt) | 정확한 revision·수정분. [파일별 비교](p0/imgui_upstream_comparison.csv). |
| ImGui 플랫폼·렌더 backend | `Framework/imgui_impl_win32.cpp`, `imgui_impl_dx11.cpp`, `imgui_impl_dx12.cpp` | 위 ImGui 후보와 파일별 비교 | 실제 선택된 렌더 경로·후보 외 revision. |
| ImGui에 포함된 stb | `Framework/imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h` | 각 헤더 끝의 MIT 또는 public-domain 선택지 | 실제 이관 파일과 선택한 고지 기록. |
| MinHook | `Hooking/buffer.c`, `hook.c`, `trampoline.c` 및 관련 헤더 | 소스 머리말 및 [공식 v1.3.3 고지](https://github.com/TsudaKageyu/minhook/blob/9fbd087432700d73fc571118d6a9697a36443d88/LICENSE.txt) | 정확한 상류 revision·실제 포함 파일. |
| HDE | `Hooking/hde32.c`, `hde64.c`, 관련 헤더·table | 파일 머리말과 위 공식 MinHook 고지의 HDE 항목 | 기준 파일과 공식 고지의 대응 revision. |
| Kiero | `Hooking/hookmain.cpp`, `hookmain.h` | 버전 표기 1.2.6, [공식 비교 후보 MIT 원문](https://github.com/Rebzzel/kiero/blob/358d2cdab40465f9fe97a4034063416e54b9d7a5/LICENSE) | 실제 링크·호출 여부와 정확한 도입 경위. |

각 소스가 프로젝트 평가에서 입력인 것과 최종 바이너리에 남는 것은 별개다. 사용자 데이터·폰트·이미지 및 NightFyre 자체 구현의 출처는 이 표로 확인되지 않는다. 기준 소스를 빌드하거나 배포하지 않았다.

### x64 평가 항목과 혼합된 ImGui 버전 단서

[evaluated_x64_compile_items.csv](p0/evaluated_x64_compile_items.csv)를 다시 확인했다. Debug·Release와 hid·dinput8·dxgi의 여섯 조합 모두에서 위 표의 ImGui cpp 8개와 Hooking 컴파일 항목 6개는 `Excluded` 값이 비어 있다. 이는 평가된 항목의 조건이며 실제 컴파일·링크 결과는 아니다. `imstb_*`는 별도 컴파일 항목이 아닌 헤더다. HDE 32·64의 아키텍처별 전처리 조건은 [빌드 입력 조사](P0_Build_Inputs.md)를 따른다.

`Framework/imgui.cpp`는 1.91.0 WIP를 표기하고 앞의 가장 유사한 `imgui.h` 후보는 2024-07-02다. 그러나 기준 `imgui_impl_dx12.cpp` 머리말에는 2024-11-15 API 변경 이력이 있다. 이는 core와 DX12 backend가 같은 7월 후보 스냅샷이라고 묶을 수 없다는 추가 단서다. 머리말 날짜만으로 정확한 backend 원본 커밋이나 호환성을 확정하지 않는다. 각 파일의 실제 컴파일·런타임 조합도 확인하지 않았다.

## MinHook v1.3.3의 전체 대응 파일 비교

공식 [MinHook v1.3.3](https://github.com/TsudaKageyu/minhook/tree/9fbd087432700d73fc571118d6a9697a36443d88)의 13개 대응 파일을 기준 `Hooking` 디렉터리와 비교했다. 13개 중 `table32.h`, `table64.h`는 UTF-8 BOM과 CRLF 차이만 정규화하면 일치한다. 나머지 11개 중 8개(`MinHook.h`, `buffer.h`, `trampoline.h`, `hde32.c/.h`, `hde64.c/.h`, `pstdint.h`)는 파일 전체의 공백과 BOM을 제거하면 텍스트가 일치한다. HDE C 파일의 줄 단위 차이는 중괄호와 `else`의 줄 배치 등 서식 차이로 나타났다.

나머지 세 파일을 공백 무시 diff로 확인한 차이는 `hook.c`의 MinHook 헤더 include 경로, `trampoline.c`의 HDE 헤더 두 개 include 경로, `buffer.c`의 `±` 문자가 포함된 주석 차이다. 전자는 기존 저장소에서 파일을 같은 디렉터리로 모은 배치와 대응한다. 이 비교는 파일 전체에서 공백을 제거하는 방식도 사용하므로 문자열·주석 안 공백의 의미까지 동등하다는 증거는 아니다. 정확한 도입 revision이나 이 비교 범위 밖의 Kiero 코드를 v1.3.3으로 확정하지 않는다. 상류 코드를 임시 위치에 읽기 전용 비교용으로 받았으며 기존 참고 소스는 변경하지 않았고 빌드하지 않았다.
## ImGui core·위젯의 후보 버전 차이

[앞에서 찾은 공식 후보](https://github.com/ocornut/imgui/tree/0ebf49b4c1fa9256a2ea002245a1186075fe7567)와 정규화 후에도 다른 파일을 줄 단위로 대조했다. 기준 `imgui.cpp`, `imgui_internal.h`, `imgui_widgets.cpp`는 트리 탐색에 `ImGuiTreeNodeStackData`, `TreeNodeStack`, `TreeHasStackDataDepthMask`를 함께 사용한다. 후보의 대응 이름은 `ImGuiNavTreeNodeData`, `NavTreeNodeStack`, `TreeJumpToParentOnPopMask`다. 내부 구조와 함수 매개변수도 달라 이 파일 중 하나만 후보 버전으로 바꾸는 것으로 동일한 조합을 유지할 수 없다. 이는 후보와의 차이이며 이 변경 전체가 저장소 작성자의 수정이라는 뜻은 아니다.

기준 Win32 backend는 후보와 달리 Super 수정자를 `VK_LWIN` 또는 `VK_RWIN`으로 판정하며, 파일 이력 주석은 이 변경을 2024-07-08로 표기한다. `imgui_demo.cpp`에도 트리 예제의 분기·버튼 등 차이가 있다. 기준 DX12 backend는 새로운 초기화 구조체와 SRV 할당 callback 인터페이스를 포함하지만, cpp의 동적 글꼴 텍스처 갱신 관련 코드는 주석 처리돼 있고 파일 머리말에도 해당 기능이 비활성이라고 표시한다. 헤더의 기능 안내만으로 활성 기능을 판정하지 않는다.

기준 `imgui.cpp`의 비 Windows 기본 shell 실행도 후보와 다르지만 현재 대상 Windows 분기는 별도로 존재한다. 이런 차이는 원본 도입 시점과 수정 이력이 섞였을 수 있으므로 전체를 하나의 상류 커밋으로 단정하지 않는다. 이관 시 core·internal·widgets 및 backend API를 일관된 버전으로 선택하고 트리 키보드 이동, Windows 키, DX12 초기화와 글꼴 텍스처 경로를 각각 확인한다. 컴파일과 UI 실행은 하지 않았다.
## ImGui 2024년 7월 상류 이력의 파일별 일치

공식 ImGui 저장소의 master 이력을 2024-06-01 이후로 임시 비교 저장소에 가져와, 2024-07-03부터 07-31까지 각 파일이 변경된 커밋의 해당 파일을 기준 `Framework` 파일과 대조했다. UTF-8 BOM과 CRLF만 정규화한 바이트 비교다. 다음 세 파일은 각각 이 구간의 한 버전과 일치한다.

| 기준 파일 | 일치한 상류 파일 버전 | 날짜 |
|---|---|---|
| `imgui.cpp` | [`1ec1f7a3de2c`](https://github.com/ocornut/imgui/commit/1ec1f7a3de2ce96bee58b2825917707cc1b7c17c) | 2024-07-07 |
| `imgui_internal.h` | [`2d0baaabe62a`](https://github.com/ocornut/imgui/commit/2d0baaabe62a4bd71bfc871933d9002425c45435) | 2024-07-03 |
| `imgui_demo.cpp` | [`6b6026b13320`](https://github.com/ocornut/imgui/commit/6b6026b133208725d1eb396aea0825450cefde44) | 2024-07-08 |

같은 방식으로 해당 기간의 파일 변경 커밋에서 `imgui.h`, `imgui_widgets.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`는 일치 버전을 찾지 못했다. 각각 95·130·1·4개 변경 커밋을 비교했다. 기간 이전에 마지막으로 변경된 버전이나 다른 브랜치에만 있는 버전은 이 부정 결과에 포함되지 않는다. 파일이 특정 커밋과 일치한다는 사실은 그 커밋에서 직접 가져왔거나 전체 디렉터리가 동일한 하나의 snapshot임을 뜻하지 않는다. 파일별 변경 이력은 [공식 저장소](https://github.com/ocornut/imgui)에서 확인할 수 있다. 기존 참고 소스는 변경하지 않았고 빌드하지 않았다.

### 7월 8일 snapshot에서 변경되지 않은 파일과 변경점

앞 절은 기간 안에 **해당 파일이 변경된 커밋**만 비교했으므로 기간 시작 전에 마지막으로 변경된 파일은 놓칠 수 있다. 이를 보완해 [2024-07-08 상류 snapshot `6b6026b13320`](https://github.com/ocornut/imgui/tree/6b6026b133208725d1eb396aea0825450cefde44)의 일곱 core·demo 파일을 같은 방법으로 직접 비교했다. 기준 `imgui.cpp`, `imgui_internal.h`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_demo.cpp` 다섯 파일이 이 snapshot과 일치한다. 특히 앞 절에서 변경 커밋 검색만으로 일치를 찾지 못했던 draw와 tables도 이 snapshot의 파일과 일치한다.

`imgui.h`는 같은 snapshot에 `SmallCheckbox`, `CustomSliderInt`, `CustomSliderScalar` 선언과 `ImGuiIO::ImeWindowHandle` 필드가 추가된 차이다. `Engine.cpp:549`는 이 필드에 게임 윈도우를 대입한다. `imgui_widgets.cpp`는 checkbox·slider 확장과 일부 위젯의 hover/held 텍스트 색상 변경 등 추가 코드가 있다. 상류 snapshot 대비 기준 트리의 차이를 확인한 것이며, 누가 어떤 시점에 변경했는지는 이 비교만으로 알 수 없다. 이관에서는 core 파일 일치뿐 아니라 수정된 헤더·widgets와 한글 IME 경로를 함께 검증해야 한다. 빌드와 UI 실행은 하지 않았다.

### Win32 수정 반영 이후 snapshot 전체 대조

Win32의 Super 키 변경은 공식 [커밋 `9504068f66c0`](https://github.com/ocornut/imgui/commit/9504068f66c00b9a16553f7da7dc6493906c88eb)에서 2024-07-08에 반영됐다. 이 snapshot의 `Framework` 대응 17개 전체를 BOM·CRLF만 정규화해 대조하면 13개가 일치한다. 원본 및 정규화 SHA-256과 파일별 결과는 [비교표](p0/imgui_9504068f_comparison.csv)에 기록했다. 일치하는 파일은 `imconfig.h`, core·demo의 `imgui.cpp`, `imgui_internal.h`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_demo.cpp`, `imstb_*` 헤더 3개, DX11 backend 2개, Win32 backend 2개다.

일치하지 않는 네 파일은 `imgui.h`, `imgui_widgets.cpp`, DX12 backend cpp·h다. 두 core 파일의 전체 텍스트 차이는 [비교 diff](p0/imgui_9504068f_core_changes.diff)에 기록했다. UTF-8 BOM과 줄바꿈을 정규화한 검토용 자료다. 상류 두 파일의 임시 복사본에서 `git apply --check`와 적용을 수행하고, 적용 결과가 줄바꿈·BOM 정규화 후 기준 두 파일과 각각 일치함을 확인했다. 참고 저장소에는 적용하지 않았다. 앞 절에서 비교한 이전 7월 8일 snapshot과 달리 이 snapshot은 Win32 cpp의 Super 키 변경까지 포함한다. 이 네 파일의 차이는 각각 자체 추가와 후속 상류 변경이 섞였을 수 있다. 단일 snapshot과의 파일 일치이며 실제 도입 이력이나 런타임 호환성을 확정하지 않는다.

### DX12 backend의 후속 상류 기능 흔적

공식 ImGui 이력에서 `ImGui_ImplDX12_InitInfo`는 [2024-11-15 변경](https://github.com/ocornut/imgui/commit/40b2286d16e92ea017347a62cb91e63f378ff455), `ImGui_ImplDX12_UpdateTexture`는 [2025-06-11 동적 텍스처 지원 변경](https://github.com/ocornut/imgui/commit/eefe5d5aac1a9040020855c3bb918fe469cf4a38)에서 도입됐다. 기준 DX12 backend에는 전자의 구조체가 있고 후자의 텍스처 갱신 코드가 주석 처리되어 있다. 이는 2024년 7월 core snapshot과 동일한 단일 시점의 backend가 아니라는 구체적 차이다.

2025-06-11 상류 DX12 헤더와 기준 헤더를 줄 단위로 비교하면 `dxgiformat.h` 직접 include 제거, 빈 줄, `ImGui_ImplDX12_UpdateTexture(ImTextureData*)` 선언을 주석으로 비활성화한 차이가 있다. cpp는 해당 시점과 추가 차이가 많아 정확한 원본 revision은 확정하지 않는다. 이 비교는 상류 기능의 등장 시점과 파일 차이를 보여줄 뿐 기준 파일의 실제 도입 경로를 증명하지 않는다. 빌드와 DX12 실행은 하지 않았다.

### DX12 backend 도입 이력과 2026-04 상류 후보

기준 저장소에서 DX12 backend cpp·h의 이력을 각각 `git log --follow`로 확인했다. 두 파일은 `bcb69bf727fe6a5be998724443f00fe559a226a6`의 2026-04-27 DX12 지원 변경에 나타나고, 마지막 변경은 `97756a10c924cace3fbd1e4cf4fc569a1beea404`의 2026-04-28 DX12 갱신이다. 후자의 commit 메시지는 공식 Git에서 갱신했다고 적지만 상류 commit ID는 밝히지 않는다. 따라서 앞의 2025-06 변경은 포함 기능의 등장 시점을 설명하며 DX12 파일 전체의 마지막 도입 시점은 아니다.

공식 이력의 [2026-04-23 callback 변경](https://github.com/ocornut/imgui/commit/0a811e8ea13a7dcf0a51a39c961d7f47f42c8786)과 그 뒤 [상류 snapshot `86ebb3bc`](https://github.com/ocornut/imgui/commit/86ebb3bc1134aa5d79ab7380c2c690eeb669a05d)을 확인했다. 기준 cpp의 sampler callback 구조가 이 시기의 상류 코드에 나타난다. `0a811e8`은 author 날짜가 2019년이지만 committer 날짜는 2026-04-23이므로 이 구분을 유지한다.

기준 cpp·h와 상류 `86ebb3bc`의 텍스트 차이는 [DX12 비교 diff](p0/imgui_86ebb3b_dx12_changes.diff)에 기록했다. 동적 texture 순회·구현과 플랫폼 callback 게시를 비활성화한 차이 등이 포함되며, 두 파일 모두 BOM·줄바꿈만 정규화해서는 일치하지 않는다. 이는 근접 후보와 파일 차이이지 실제 복사 commit을 입증하지 않는다. 참고 저장소 변경·빌드·실행은 하지 않았다.

이 DX12 diff는 상류 두 파일의 임시 복사본에서 적용 가능 여부를 확인하고 적용 결과가 줄바꿈·BOM 정규화 후 기준 두 파일과 일치하는지 대조했다. 임시 파일만 변경했다.

## 기준 저장소의 명시적 출처 표기

기준 `README.md:264-273`은 기반 프로젝트로 NightFyre의 `b44d74cbb20918c1b616190325ee49589b80cf36`을 링크하고 ImGui·MinHook 유래 구성 요소를 언급한다. 같은 파일의 출처 절에는 범위 PNG의 제작자나 획득 경로가 표기돼 있지 않다. 저장소 파일명 검사에서는 별도 LICENSE·NOTICE 파일을 찾지 못했다. 이는 기준 트리의 표기 범위이며 각 자산의 실제 도입 경로를 확정하지 않는다.
