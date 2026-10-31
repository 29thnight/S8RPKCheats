# P0 오버레이·입력 조사

기준: S8RPKCheats `068ca1d881edf870f7cf06f935cf50e286004af8`. 아래는 정적 코드 경로이며 실제 Windows 게임에서의 입력·장치 검증은 아직 하지 않았다.

## 그리기와 자원

- `Engine.cpp:202`의 Present hook은 `IsAnyUIOpen()`이 참일 때만 `Overlay`를 호출하고 원본 Present를 호출한다. `MenuState.cpp:227`의 이 판정은 메인 메뉴 외 무장·도시·책략·디버그 등 개별 창도 포함한다. UI가 모두 닫힌 동안에는 `Overlay`와 ImGui frame이 실행되지 않는다.
- `Engine.cpp:307`의 첫 `InitImGui`는 전달받은 swap chain을 raw 포인터로 보관하고 DX11 또는 DX12 device를 조회한다. DX11은 immediate context, back buffer와 render target view, ImGui Win32/DX11 backend, 11개 범위 이미지 순으로 준비한다. `GetBuffer`, `CreateRenderTargetView`, backend 초기화의 결과를 이 경로에서 모두 검사하지는 않는다.
- `Engine.cpp:491`은 이미지 SRV를 `g_RangeTextures[1..11]`에 저장하고 `TacticsEditWindow.cpp`가 이미지 버튼에 사용한다. 자체 cpp/inc/h의 `g_RangeTextures` 참조 검색과 `RenderManager` 소멸자에서 이 배열의 SRV를 Release하는 경로는 찾지 못했다. 자원 소유와 재초기화·종료 처리를 이관 시 명시한다.
- `RenderManager` 소멸자는 ImGui backend/context를 종료하고 DX11 device·context·RTV 및 DX12 자원을 Release한다. `m_pSwapChain` 저장 시 AddRef와 Release는 확인되지 않았다. 현재 swap chain, 장치 변경과 callback 종료의 수명 관계는 검증이 필요하다.
- ResizeBuffers callback은 구현되어 있으나 `HookD3D`는 Present만 설치한다. DX12 분기와 command queue hook 코드가 있으나 사용자 대상 기준은 DX11이며 DX12가 동작한다고 판단하지 않는다.

## 입력 라우팅

- `Source_impl.inc:72`의 `GetAsyncKeyState`, `GetKeyState`, `GetKeyboardState` hook은 UI가 열리고 ImGui가 키보드 캡처를 원하는 경우 게임에 주는 키 상태를 줄인다. Shift/Ctrl/Alt 계열, 한영 키, 메뉴 토글 키는 예외로 남긴다.
- `PeekMessageA/W` hook은 `PM_REMOVE`인 성공 결과에 대해 `HandleMessageCapture`를 호출한다. Raw Input에서 마우스 캡처 시 메시지를 `WM_NULL`로 바꾸며, 키보드는 텍스트 입력 및 예외 키 조건을 따로 적용한다. 1024바이트를 넘는 raw input은 이 버퍼 처리 경로에서 변경하지 않는다.
- `GetCursorPos` hook은 UI·마우스 캡처·창별 차단 조건을 만족하면 좌표를 `(-1,-1)`로 바꾼다. `WndProc`의 마우스 차단 조건과 창 목록이 완전히 같지는 않으므로 실제 창별 동작을 확인한다.
- `Engine.cpp:89`의 WndProc은 UI가 열리면 IME와 키보드 메시지를 처리하고, 마우스 캡처 또는 hard-block 조건에서 일부 마우스 메시지를 차단한다. 그 밖에는 ImGui Win32 handler를 거친 후 원래 WndProc 또는 `DefWindowProc`에 전달한다. backend는 `WM_SETFOCUS`·`WM_KILLFOCUS`를 ImGui focus 이벤트로 전달한다.

## 한글 입력과 DPI

- `WM_IME_SETCONTEXT`에서 조합창 표시 플래그를 설정한다. 한영 키에서 입력 컨텍스트의 변환 모드를 전환하고 `ImmReleaseContext`를 호출한다. `WM_IME_COMPOSITION` 결과 문자열의 비ASCII UTF-16 단위를 ImGui에 넣고, 중복 입력을 피하려 `WM_IME_CHAR`와 비ASCII `WM_CHAR`를 차단한다.
- `Engine.cpp:125`는 결과 크기를 `<256` 바이트로 검사하지만 실제 `wchar_t buf[64]`는 Windows에서 128바이트이고, 순회 횟수는 최초 조회한 `byteLen / sizeof(wchar_t)`다. 조회 크기가 128바이트를 초과하면 배열 범위를 넘어 읽을 수 있다. 두 번째 `ImmGetCompositionStringW` 결과도 순회 횟수에 반영하지 않는다. 새 입력 경계에서 실제 복사 크기를 기준으로 처리하고 긴 조합 결과를 검증한다.
- `Overlay`에서 `WantTextInput`이 false에서 true로 바뀌면 한국어 입력 레이아웃 활성화, 창 활성화·포커스, IME context 연결과 조합창 위치 설정을 수행한다. 재진입과 사용자 입력 언어·포커스에 미치는 효과를 실제 호스트에서 확인한다.
- `InitImGui`는 `GetDpiForWindow`를 한 번 조회하여 scale을 구하고 글꼴 크기에 적용한다. `Source_impl.inc`의 프로세스 DPI awareness 설정 호출은 주석 처리되어 있다. 자체 코드에서 `WM_DPICHANGED` 처리 검색 결과는 없었다. 모니터 이동과 배율 변경은 별도 회귀 시나리오다.

## 회귀 기준

메인 메뉴와 개별 창의 열기·닫기, 게임 클릭 허용 설정, Raw Input과 일반 메시지, 수정키·한영 전환·메뉴 토글, 한글 조합 중 편집 및 긴 결과, 포커스 상실·복귀, DPI 변경, 이미지 표시, swap chain/장치 변경, 초기화 실패 및 정상 종료를 확인한다. 실제 장치와 게임 callback 순서를 확인하기 전까지 이러한 동작을 검증 완료로 표시하지 않는다.

## 범위 이미지 실패와 COM 초기화

`TextureLoader.cpp`의 파일·리소스 로더는 각각 별도 함수 정적 `com_init`으로 `CoInitializeEx(COINIT_MULTITHREADED)`를 최초 한 번 호출하고 HRESULT와 관계없이 flag를 올린다. 같은 함수의 이후 호출이 다른 스레드에서 일어나도 해당 스레드에서 다시 초기화하지 않으며, 이 파일에는 대응 `CoUninitialize`가 없다. WIC factory·decoder·converter와 리소스 경로의 임시 texture는 `ComPtr`로 보관한다. 파일 경로는 임시 texture를 SRV 생성 직후 명시적으로 Release한다.

리소스 로더는 이미지 크기를 얻은 뒤 pixel 복사·texture·SRV 생성 전에 출력 크기를 먼저 쓴다. 그 이후 실패할 수 있으므로 크기 출력만으로 로드 성공을 판단할 수 없다. `Engine.cpp`의 11개 로드 반복은 성공한 SRV만 배열에 기록하고 일부 실패를 그대로 건너뛰지만 반복 뒤에는 성공 로그를 무조건 출력한다. 텍스처가 없는 슬롯의 전법 화면은 숫자 버튼으로 대체한다. 재초기화가 다시 성공할 때 기존 배열의 같은 슬롯을 먼저 Release하는 코드는 이 반복에 없다. 실제 재초기화 빈도와 장치 교체 동작은 확인하지 않았다.

파일 로더는 리소스 로더와 달리 SRV 생성까지 성공한 후에만 출력 크기를 쓴다. 기준 자체 cpp·h·inc의 직접 호출 검색에서는 파일 로더 사용처를 찾지 못했고, 리소스 로더는 `Engine.cpp:491`에서 호출한다. 이 검색은 외부 또는 간접 호출을 배제하지 않는다.

회귀 조건은 리소스 부재·손상, WIC/texture/SRV 생성 단계별 실패, 이미 초기화된 다른 COM apartment 및 다른 스레드 호출, 일부 이미지 실패 후 숫자 fallback, 장치 재생성과 종료다.

## 보조 창과 유지 작업의 렌더 진입 조건

`Menu::Render`는 `g_Engine->bShowMenu`가 true일 때만 `DrawMenu`를 호출한다. `DrawMenu`의 메인 창 표시 분기가 끝난 뒤에 자녀 갱신, 무장·도시·자녀·알림·전법 보조 창 호출이 있으므로 메인 메뉴가 접혀 있어도 이 호출은 이루어진다. 그러나 메인 메뉴 자체가 닫히면 이 호출은 이루어지지 않는다. 해당 보조 창 flag가 `IsAnyUIOpen`에 남아 Present에서 overlay가 호출되는 것과 보조 창 내용이 실제로 그려지는 것은 별개다.

반면 전투 환경 창·지형 설정 창·메모장은 `Menu::Render`에서 `DrawMenu` 밖에도 호출된다. 메모장은 `DrawMenu` 안에도 호출되어 메인 메뉴 표시 시 두 번 호출되는 경로가 있다. 랜덤 기재 및 기재 텍스트 자동 적용은 더 안쪽의 메인 메뉴 확장·gameBase·테이블 조건을 따른다. 각 경로의 독립 동작은 위 소스 호출 범위의 사실이며 실제 프레임 표시와 사용자 입력은 확인하지 않았다.

## 게임 클릭 허용과 세 입력 경로의 차이

`Engine.cpp`의 WndProc은 UI가 열려 있을 때 `WantCaptureMouse`가 true면 게임 클릭 허용 설정이나 디버그 표시와 무관하게 지정된 마우스 메시지를 소비한다. `WantCaptureMouse`가 false여도 게임 클릭 허용이 OFF이고 디버그가 닫혀 있으며 hard-block 대상이면 같은 메시지를 소비한다. hard-block 대상에는 펼친 메인 메뉴, 주인공·선택 무장 창, 클릭 차단이 켜진 무장 목록·메모리 편집기가 포함된다.

`Source_impl.inc`의 `GetCursorPos` hook은 반대로 UI·`WantCaptureMouse`·게임 클릭 허용 OFF·디버그 닫힘·hard-block을 모두 요구한다. 이 hard-block에는 명품 창이 포함되지만 주인공·선택 무장 창은 별도로 들어 있지 않다. 원본 `GetCursorPos`가 실패했더라도 이 조건이면 출력 좌표를 -1로 바꾸고 원래 반환값을 유지한다. Raw Input의 마우스 메시지는 `WantCaptureMouse`만으로 `WM_NULL`로 바뀌며 게임 클릭 허용과 hard-block을 검사하지 않는다. 세 경로의 옵션 효과가 일치한다고 가정하면 기존 입력 동작이 달라진다.

회귀 조건은 클릭 허용 ON/OFF, 디버그 표시, 각 보조 창, ImGui 마우스 캡처 ON/OFF, 일반 메시지·커서 조회·Raw Input 및 원본 커서 조회 실패다. 실제 게임 입력은 확인하지 않았다.

## ImGui 변경과 현재 입력·표시 경로

[상류 후보와의 파일 비교](P0_Proxy_Origin.md#imgui-wip-공개-이력과-파일별-비교)에서 `imgui_impl_win32.cpp`의 정규화 후 차이는 변경 이력 주석과 Super 수정키의 가상 키 판정이다. 후보는 `VK_APPS`를 보지만 현재 파일은 `VK_LWIN` 또는 `VK_RWIN`을 본다. 현재 파일은 해당 변경 날짜를 2024-07-08로 표기하므로, 2024-07-02의 `imgui.h`에 가까운 후보와 모든 파일이 같은 시점이라는 가정은 맞지 않는다. 실제 입력 결과는 실행하지 않았다.

IME 창 핸들에는 두 경로가 있다. `ImGui_ImplWin32_Init`은 main viewport의 `PlatformHandle`과 `PlatformHandleRaw`에 창 핸들을 설정한다(`Framework/imgui_impl_win32.cpp:175`). 기본 IME callback은 `PlatformHandleRaw`에서 HWND를 얻어 조합창과 후보창 위치를 지정한다(`Framework/imgui.cpp:14416`). `Engine.cpp:549`가 별도로 쓰는 추가 `io.ImeWindowHandle`은 앞의 기본 callback이 읽는 필드가 아니다. `Engine.cpp`에는 WndProc 조합 결과 처리와 텍스트 입력 시작 시 별도의 조합창 위치 설정도 있다. 이관 시 한 필드만 보고 IME 경로 전체를 판단하지 않는다.

`imgui_widgets.cpp`는 후보 상류와 비교해 버튼, 선택 항목, 탭의 hover·선택 상태에서 텍스트 색상을 `(51,38,12,255)`로 임시 변경하는 코드를 포함한다. 각 분기는 비활성 항목에 이 색을 적용하지 않고 렌더 후 원래 색을 복구한다. 구체적으로 버튼은 `hovered || held`, 선택 항목은 `hovered || selected || held`, 탭은 `hovered || tab_contents_visible || held`에서 이 색을 사용한다. 선택 항목은 개별 disabled와 전역 disabled를 모두 제외하고, 버튼·탭은 현재 item의 disabled flag를 제외한다. 세 위젯의 표시가 이 공통 구현에 의존하므로 ImGui 파일을 상류로 교체하면 화면의 기존 색상 동작도 함께 달라진다. 같은 파일에는 `SmallCheckbox`와 사용자 정의 슬라이더 구현이 있으나 자체 코드에서 직접 호출은 찾지 못했다. 이 비교는 시점이 다른 상류 변경도 포함하므로 모든 차이를 프로젝트 자체 수정으로 단정하지 않는다.

회귀 시나리오는 왼쪽·오른쪽 Windows 키와 메뉴 키, 조합 중 커서 이동 및 후보창 위치, 버튼·목록·탭의 활성과 비활성 색상이다. 실제 입력과 화면은 확인하지 않았다.

## UI 배율 조작과 저장

메인 메뉴 하단의 `-`와 `+`는 `ImGuiIO::FontGlobalScale`을 0.1씩 바꾸고 각각 0.5와 3.0에서 제한한다. 가운데 슬라이더도 같은 필드를 0.5~3.0으로 바꾸지만, 슬라이더 변경 시에만 `SaveConfig`를 호출한다. 기준 자체 cpp·h·inc에서 이 필드의 설정 저장·로드 호출은 찾지 못했다. `SaveConfig`가 호출되더라도 현재 설정 파일에 이 배율을 기록하는 경로는 확인되지 않으며, ImGui context 초기화에서는 `io.IniFilename=0`으로 ImGui의 기본 ini 저장도 끈다. 실제 종료·재시작 후 표시 결과는 실행하지 않았다.

메뉴 버튼과 창 크기에 사용하는 `scale`은 `GetDpiForWindow`로 매 렌더 계산하고, 첫 글꼴 크기는 초기화 시의 DPI로 정한다. `FontGlobalScale`은 별도 필드다. 재야 알림과 로그 표시의 일부 코드에서는 이 필드를 별도 배율로 읽는다. 따라서 현재 UI 배율 조작을 DPI·모든 레이아웃 크기·저장값을 하나로 변경하는 기능으로 취급하지 않는다.

회귀 조건은 0.5·3.0 경계, 버튼과 슬라이더 변경, 다른 설정 저장 뒤 재시작, DPI 변경, 글꼴과 위젯 크기 및 재야 알림·로그의 표시다. 위 내용은 소스 경로의 정적 확인이다.

## 조건부 DX12 명령 큐 캡처

`Engine.cpp:307`의 DX12 분기는 swap chain에서 `ID3D12Device`를 얻은 뒤 아직 명령 큐가 없으면 임시 DIRECT 큐를 만들어 그 vtable의 `ExecuteCommandLists`를 hook하고 임시 큐를 Release한다. hook 생성 반환값은 확인하지 않으며 그 프레임의 `InitImGui`는 false를 반환한다. UI가 열려 Present에서 `Overlay`가 계속 호출되면 초기화되지 않은 다음 프레임에 이 과정을 다시 시도할 수 있다. 실제 게임 큐의 해당 함수가 같은 hook 대상에 도달하는지, 캡처가 성공하는지는 실행하지 않아 확인되지 않았다.

hook callback은 처음 관찰한 DIRECT 큐 포인터를 `m_pd3dCommandQueue`에 저장하고 원본 `ExecuteCommandLists`를 호출한다. 이 저장 경로에는 큐의 `AddRef`가 없고 `RenderManager` 소멸자에도 해당 큐의 `Release`는 없다. 따라서 포인터의 수명을 게임 측에 의존한다. 큐가 준비되지 않은 상태로 여러 번 `InitImGui`가 실행되면 각 성공적인 `GetDevice`가 같은 멤버에 다시 쓰이며 그 사이 이전 `m_pd3dDevice12`를 Release하지 않는다. 소멸자는 마지막으로 보관된 포인터를 한 번 Release한다. 이 경로는 현재 확인한 코드의 자원 보관 방식이며 실제 DX12 호출 빈도나 참조 누수를 실행해 확인한 것은 아니다.

회귀 조건은 UI를 연 상태의 연속 초기화 실패, 임시 큐·hook 생성 실패, DIRECT 외 큐 호출, 여러 DIRECT 큐와 첫 캡처, 실제 큐 교체·해제, DX12 장치 참조와 종료다. 사용자 기준의 DX11 동작과 이 조건부 DX12 코드를 구분한다.

### DX12 backend 선언과 실제 활성 경로

기준 `imgui_impl_dx12.h` 머리말은 동적 texture 업데이트와 `GetPlatformIO().Renderer_RenderState` 노출을 지원한다고 표시한다. 그러나 대응 cpp는 texture 업데이트 순회·해제 코드를 주석 처리했고 `ImGuiBackendFlags_RendererHasTextures`도 설정하지 않는다. 렌더 상태를 backend 내부 `RenderState`에 설정하지만 `platform_io.Renderer_RenderState`로 게시하는 줄은 주석이다. 헤더의 기능 목록을 현재 cpp의 활성 동작으로 그대로 취급하지 않는다.

`Engine.cpp:527`의 DX12 경로는 `ImGui_ImplDX12_InitInfo`에 장치·캡처한 command queue 등을 넣어 새 형태의 초기화를 호출한다. 해당 backend는 초기화에서 queue가 null이 아님을 assert한다. core와 backend를 교체할 때에는 이 초기화 구조, descriptor 할당 경로, 현재 비활성인 texture 갱신·공개 render state를 함께 대조해야 한다. 실제 DX12 초기화·렌더는 실행하지 않았고, 위 내용은 빌드 성공이나 기능 동작의 증거가 아니다.

### DX12의 단일 SRV와 초기화 반환값

`Engine.cpp:505`는 DX12 shader-visible SRV heap을 디스크립터 1개로 만들고, `ImGui_ImplDX12_InitInfo`에 할당·해제 callback 대신 그 heap의 첫 CPU/GPU 핸들을 legacy 필드로 전달한다. backend는 obsolete 함수 지원이 켜져 있으면 callback이 없는 경우 단일 디스크립터용 callback을 채운다. 이 callback은 동시에 한 개만 사용한다는 내부 flag를 확인한다. `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`를 켜면 해당 legacy 필드와 대체 경로가 조건부로 제거되므로 현재 호출과 함께 검토해야 한다.

범위 이미지 11개의 `LoadTextureFromResource` 호출은 DX11 분기 안에만 있다. DX12 분기는 별도 범위 이미지 로드를 수행하지 않는다. 기존 UI의 범위 선택에는 텍스처가 없을 때 숫자 버튼으로 대체하는 경로가 있다. 실제 DX12에서 해당 자원이 어떤 상태인지 실행 결과는 확인하지 않았다.

DX12 분기는 SRV·RTV heap 생성 실패는 반환하지만, Win32/backend 초기화 및 command allocator·command list 생성의 반환값을 해당 코드에서 확인하지 않고 계속한다. 마지막에 `bInitImGui=true`를 설정한다. 이관 확인은 단일 디스크립터 사용, obsolete 옵션, 범위 이미지 대체 표시, 각 초기화 실패 및 부분 생성 자원의 정리다.

### DX12 back buffer 범위와 allocator 재사용

`Engine.h`의 command allocator·back buffer resource·RTV descriptor 배열은 각각 8칸이다. `InitImGui`는 swap chain의 `BufferCount`를 최대 8로 잘라 그 수만큼 초기화하지만, `Overlay`는 `GetCurrentBackBufferIndex()`의 반환값을 별도 범위 검사 없이 세 배열의 인덱스로 사용한다. 실제 swap chain의 buffer 수가 8보다 크고 현재 인덱스가 8 이상인 경우 이 코드 경로는 배열 범위를 벗어난다. 그런 swap chain이 실제 사용되는지는 확인하지 않았다.

매 DX12 overlay 호출은 선택한 allocator와 command list를 reset하고 기록·제출한다. `Engine.cpp`에는 제출한 작업의 완료를 확인하는 fence 대기가 보이지 않으며 reset 등의 반환값도 검사하지 않는다. 해당 allocator의 GPU 사용이 이미 끝났는지는 이 코드만으로 확정할 수 없다. 회귀 조건은 buffer 수 1·8·9, 인덱스 경계, 연속 Present와 GPU 작업 완료 전 재사용, reset 실패와 장치 교체다. DX12 실행은 하지 않았다.

### DX12 heap 실패 후 ImGui context 재시도

DX12 queue가 확보된 뒤 `InitImGui`는 `ImGui::CreateContext()`를 호출한 다음 SRV·RTV heap을 만든다. heap 생성이 실패하면 `bInitImGui`를 true로 만들기 전에 false를 반환하며 이 함수에서 방금 만든 context를 파괴하지 않는다. `Overlay`는 이후 호출에서도 `bInitImGui`가 false이면 초기화를 다시 시도한다.

기준 `imgui.cpp:3668`의 `CreateContext`는 이전 current context가 있으면 새 context를 초기화한 뒤 이전 context를 다시 current로 복원한다. 반환한 새 context는 `Engine.cpp`에서 저장하지 않는다. 따라서 첫 heap 실패 후 재시도하면 새 context가 만들어지지만 이후 `GetIO()`는 기존 current context를 사용할 수 있고, 새 context를 나중에 파괴할 핸들은 이 호출자에 남지 않는다. RTV heap 실패는 그 전에 생성한 SRV heap도 이미 존재하는 경로다. 소멸자의 ImGui 종료·context 파괴는 `bInitImGui`가 true일 때만 수행한다.

이것은 실패 경로의 정적 소유 분석이며 실제 생성 실패나 재시도를 실행한 결과는 아니다. 회귀 조건은 첫 SRV 실패, SRV 성공 후 RTV 실패, 연속 실패 후 성공, 이전 current context 존재 및 종료 시 부분 생성 자원이다.

### swap chain 장치 판별 실패

`RenderManager`의 초기 `m_RenderType`은 `None`이다. `InitImGui`는 DX11 장치 조회가 성공하면 DX11, 아니면 DX12 조회가 성공하면 DX12로 바꾸지만 두 조회가 모두 실패했을 때 반환하는 분기는 없다. 이어 context와 swap chain 설명을 사용하고, 리소스 생성에서 `m_RenderType == DX11`이 아니면 DX12 분기로 들어간다. 최초 호출에서 두 조회가 실패하면 DX12 장치 포인터는 초기 null 상태인데 그 분기의 `CreateDescriptorHeap` 호출에 사용된다.

`GetDesc` 반환값도 확인하지 않은 채 창 핸들과 버퍼 수를 사용한다. 이 코드는 모든 swap chain이 반드시 두 장치 중 하나로 해석된다거나 설명 조회가 성공한다는 별도 확인을 제공하지 않는다. 실제 판별 실패 발생 여부는 확인하지 않았다. 이관 회귀 조건은 DX11·DX12 조회 각각 성공, 두 조회 실패, swap chain 설명 조회 실패 및 장치 교체 후 재초기화다.

### DX11 backend의 부분 초기화 실패

`Engine.cpp:480`은 back buffer 포인터 변수를 초기화하지 않은 채 `GetBuffer`에 전달하고 반환값을 검사하지 않는다. 이어 render target view 생성 결과와 무관하게 그 포인터의 `Release`를 호출한다. RTV 생성 실패 시에도 Win32·DX11 backend 초기화와 이미지 로드를 계속하며 최종적으로 `bInitImGui`를 true로 설정하는 경로다.

기준 `ImGui_ImplDX11_Init`은 device→DXGI device→adapter→factory 조회가 모두 `S_OK`일 때 backend의 device/context를 설정하지만, 그 뒤 조회 실패 여부와 무관하게 backend device와 context에 `AddRef`를 호출한다. Win32 backend는 성능 카운터 초기화 실패 시 backend 데이터를 만들기 전에 false를 반환한다. Engine은 두 초기화 반환값을 검사하지 않고 완료 상태를 기록하며, 이후 `NewFrame`과 종료 시 해당 backend가 초기화됐다고 가정한다.

이관 회귀 조건은 back buffer·RTV 생성 실패, 각 DXGI 상위 객체 조회 실패, Win32 카운터 조회 실패, backend 일부만 초기화된 상태의 종료, 재시도와 부분 생성 자원이다. 실제 실패나 충돌은 실행해 확인하지 않았다.

### 창 프로시저 설치·복원의 소유 확인

`InitImGui`는 `SetWindowLongPtr(GWLP_WNDPROC)`의 반환값을 이전 프로시저로 보관하고 별도의 성공 확인 없이 초기화 완료를 기록한다. WndProc은 보관한 이전 프로시저가 있으면 `CallWindowProc`으로 넘기고 없으면 `DefWindowProc`을 호출한다.

정상 종료의 `UnhookD3D`와 프로세스 종료 분기의 복원은 현재 창 프로시저가 여전히 자기 `WndProc`인지 확인하지 않은 채 보관한 이전 프로시저를 다시 설정한다. 반환값도 확인하지 않고 `m_OldWndProc`을 null로 지운다. 따라서 다른 구성 요소가 그 사이 창 프로시저를 교체한 경우 그 변경을 덮을 수 있으며, 복원 호출이 실패해도 저장된 이전 포인터는 이 경로에서 사라진다. 실제 창 교체 순서와 실패 여부는 확인하지 않았다.

회귀 조건은 설치 실패, 다른 subclass의 선행·후행 설치, 정상 종료와 프로세스 종료, 복원 실패, 창 교체 및 WndProc 실행 중 종료다.

### IME 활성화·비활성화 메시지의 기존 전달값

UI가 하나라도 열린 동안 WndProc의 `WM_IME_SETCONTEXT` 분기는 전달받은 `lParam`에 조합창 표시 bit를 추가하고, 원래 `wParam`과 관계없이 `TRUE`를 인자로 `DefWindowProc`을 호출한 뒤 반환한다. 따라서 비활성화를 나타내는 입력 메시지도 이 분기의 기본 처리에는 활성화 값으로 전달된다. UI가 모두 닫힌 경우에는 이 특수 분기를 거치지 않는다.

같은 열린 UI 조건에서 `WM_IME_COMPOSITION`은 결과 문자열을 별도 입력 처리한 뒤 원래 WndProc이 아니라 `DefWindowProc`으로 전달하고 반환한다. 일반 키보드 메시지 범위도 ImGui handler 뒤 `DefWindowProc`으로 반환한다. 실제 Windows IME가 이 전달값에 어떻게 반응하는지는 실행하지 않아 확인되지 않았다. 회귀 조건은 포커스 획득·상실의 `WM_IME_SETCONTEXT` 값, UI 열림·닫힘, 조합 중 창 전환과 기존 WndProc의 메시지 수신 여부다.

### 키 상태 조회와 Raw Input의 키별 차이

UI가 열리고 `WantCaptureKeyboard`가 true일 때 `GetAsyncKeyState`·`GetKeyState`는 수정키, 메뉴 토글, 한영·오른쪽 Alt를 제외한 키에 0을 반환한다. `GetKeyboardState`는 원본 호출이 성공한 뒤 256바이트 전체를 0으로 만들고 같은 수정키·메뉴 토글·한영/오른쪽 Alt의 저장된 값을 복원한다. 원본 호출이 실패하면 버퍼를 이 분기에서 바꾸지 않는다.

Raw Input은 `PeekMessageA/W`가 `PM_REMOVE`로 메시지를 가져오고 데이터 크기·읽기 조건을 통과한 경우에만 검사한다. 키보드 캡처 중 텍스트 입력을 원하면 수정키·한영/오른쪽 Alt·`VK_PROCESSKEY`·메뉴 토글을 제외한 키를 `WM_NULL`로 바꾼다. 텍스트 입력 여부와 관계없이 Space와 Backspace도 `WM_NULL`로 바꾼다. 따라서 `VK_PROCESSKEY`는 Raw Input의 텍스트 입력 차단 예외이지만 위 키 상태 조회의 별도 예외는 아니다. 실제 게임이 어느 API를 사용하는지는 확인하지 않았다.

회귀 조건은 세 키 상태 API와 Raw Input의 동일 키 비교, `VK_PROCESSKEY`, Space·Backspace, 수정키, 메뉴 토글, 텍스트 입력 여부, `PM_NOREMOVE`와 원본 조회 실패다.

### 키 상태 API의 원본 호출 순서

키보드 캡처 조건에서 차단 대상인 `GetAsyncKeyState`와 `GetKeyState`는 원본 함수를 호출하지 않고 즉시 0을 반환한다. 반면 `GetKeyboardState`는 항상 원본을 먼저 호출하고 성공한 결과 배열만 필터링한다. `PeekMessageA/W`와 `GetCursorPos`도 원본을 먼저 호출한 뒤 각각 메시지 또는 좌표를 수정하며 원본 반환값을 유지한다. 따라서 동일한 차단 표시라도 원본 API가 호출됐는지와 원본의 상태 변화가 발생할 수 있는지는 경로마다 다르다.

`GetCursorPos`는 원본이 실패한 경우에도 차단 조건이 참이면 `lpPoint`에 -1을 쓴다. 이 함수에서 출력 포인터를 별도로 검사하지 않는다. 이는 현재 호출 순서와 인자 사용에 대한 정적 조사이며, 실제 게임의 호출 인자나 OS 내부 상태는 확인하지 않았다. 회귀 조건은 차단·통과 각각의 원본 호출 여부, 실패 반환 유지, 256바이트 상태 필터, 메시지 제거 여부와 입력 이벤트 순서다.

## 숫자 입력 팝업의 현재 연결

`showcal.cpp`의 숫자 팝업은 전달된 정수 포인터의 값을 버튼이나 키 입력마다 직접 바꾼다. 숫자는 기존 값이 100000000보다 작을 때 덧붙이고, C/Delete는 0, 뒤로/Backspace는 10으로 나눈다. 확인 또는 Escape는 `pSelectedVar`를 비우고 팝업을 닫으며, Escape에도 원래 값을 되돌리는 처리는 없다.

`Menu_impl.inc:1082`는 `pSelectedVar`가 null이 아닐 때 팝업을 열지만, 기준 자체 cpp·h·inc에서 이 포인터는 null로 초기화하고 팝업 종료 시 null로 바꾸는 대입만 찾았다. 선언·참조를 포함한 텍스트 검색 범위에서는 메뉴가 유효 포인터를 지정하는 경로를 찾지 못했으므로 현재 접근 가능한 입력 기능으로 단정하지 않는다. 외부 코드의 메모리 변경과 매크로로 생성되는 대입은 이 검색만으로 배제하지 못한다. 이관할 때 팝업 구현의 존재와 실제 진입 경로를 구분한다.
## 충성도 옵션의 비활성화 범위

`MenuSectionsSocial.cpp:205-223`은 충성도 체크박스 앞뒤에서 `g_loyaltyThreadRunning`을 각각 다시 읽어 첫 값이 true면 `BeginDisabled`, 두 번째 값이 true면 `EndDisabled`를 호출한다. 두 값은 같은 지역 snapshot이 아니다. `SetInstantLoyalty(true)`는 이 flag를 true로 바꾸고 worker를 시작하며, worker 종료 또는 생성 실패 시 false로 바꾼다.

따라서 처음 false로 읽은 뒤 체크박스 ON으로 worker가 시작되고 두 번째 읽기까지 실행 중이면, 이 범위의 `BeginDisabled` 없이 `EndDisabled`가 호출된다. 반대로 처음 true여서 시작한 뒤 worker가 끝나 두 번째 읽기가 false면 대응하는 `EndDisabled`가 호출되지 않는다. 포함된 ImGui의 `EndDisabled`는 비활성 스택 크기가 양수인지 assert한 뒤 항목 플래그 스택을 pop한다. 두 호출의 불일치는 실제로 이 경로가 선택됐을 때 UI 스택 상태에 영향을 준다. 반면 인접한 공명 체크박스는 `resonanceBusy` 지역값을 두 조건에 공유한다.

이관 시 한 번 읽은 작업 상태로 UI 범위를 짝지어 처리하고, 설치 완료·생성 실패와 체크 직후 상태 변경을 확인한다. 이 결과는 정적 코드 분석이며 실제 작업 완료 타이밍이나 UI 오류를 실행해 재현한 것은 아니다. 기존 참고 소스는 수정하지 않았다.
## 배속 선택 경계에서의 비활성화 범위

`Menu_impl.inc:858-876`은 `-` 버튼 앞에서 `s_speedPending <= 1.0f`이면 `BeginDisabled`를 호출하고, 버튼 처리 후 같은 조건을 다시 평가해 `EndDisabled`를 호출한다. 활성 상태인 1.5배에서 `-`를 누르면 중간에 선택값이 1.0배가 되므로 시작 없이 종료를 호출하는 경로다. `+` 버튼도 4.5배에서 5.0배로 바뀌면 같은 불일치가 생긴다. 이 경우는 스레드 타이밍과 무관하게 버튼 처리 자체가 두 조건 사이의 값을 변경한다.

앞 절의 충성도 경로와 마찬가지로 기존 `ImGui::EndDisabled`는 시작된 비활성 스택을 전제로 한다. 적용 버튼은 별도의 `hasPendingChange` 지역값을 시작과 종료 양쪽에서 사용한다. 이관 시 각 +/- 버튼의 비활성 여부를 버튼 처리 전에 한 번 정해 같은 값으로 범위를 닫고, 1.5→1.0과 4.5→5.0 및 이미 경계에 있는 경우를 확인한다. UI 실행으로 오류를 재현하지는 않았다.
## 자동 성장 설정과 일괄 기재 창의 범위 비교

`MenuSectionsOfficerEdit.cpp:404-414`의 자동 특수능력 옵션은 `bAIOfficerAutoGrowth`를 앞뒤에서 읽어 `BeginDisabled`와 `EndDisabled`를 조건부 호출한다. 이 bool은 `Config.cpp:483-485`에서 ClientBGThread의 초기 설정 로드가 갱신할 수 있고, 메뉴 렌더에서도 자동 성장 체크박스가 변경한다. 초기 로드와 렌더가 겹칠 때 두 접근은 공통 동기화가 없는 일반 bool 접근이다. 따라서 이전 절의 배속 버튼처럼 동일 스레드에서 값이 바뀐다는 확정 경로와 구분해야 하지만, 해당 범위의 짝을 맞추고 설정을 안전하게 게시하는 작업이 필요하다. 실제 경합은 실행하지 않았다.

반면 `SelectOfficercapture_impl.inc:1271-1295`의 일괄 기재 등급 선택은 `s_batchRandomJob.running`을 양쪽에서 읽는다. 이 bool의 변경은 렌더 경로의 `TickBatchRandomTraitJob` 및 실행 버튼 처리에 있으며, 별도 수집 worker는 `s_batchRandomCollectRunning`, `s_batchRandomCollectDone`과 mutex로 보호된 결과 벡터를 사용한다. 해당 두 읽기 사이의 등급 체크박스는 `running`을 바꾸지 않는다. 이 소스에서 확인한 호출 흐름만으로는 충성도 옵션과 같은 worker의 직접 변경 사례로 분류하지 않는다. 렌더 경로의 재진입이나 확인 범위 밖의 변경은 이 대조에 포함하지 않는다.

자체 cpp·h·inc에서 `BeginDisabled`와 `EndDisabled` 표기를 확인했고, 도시 변환 4개와 공명·특수능력 자동배정은 상태를 지역값에 보관해 양쪽에 사용한다. 이 검색은 전처리·제어 흐름 전체를 증명하지 않으며, 위 세 지점 이외에 모든 ImGui 스택 호출이 안전하다는 결론은 아니다.
## DX12 렌더 상태 복구 callback의 core 불일치

기준 `Framework/imgui.h:2714`는 `ImDrawCallback_ResetRenderState`를 특수 함수 포인터 값 `(ImDrawCallback)(-8)`으로 정의한다. DX11 backend는 이 값을 비교해 렌더 상태를 다시 설정한다. 반면 `imgui_impl_dx12.cpp:216,331`은 같은 이름의 core 상수가 아니라 backend 내부의 빈 함수 `ImGui_ImplDX12_DrawCallback_ResetRenderState` 주소를 비교하고, 그 외의 callback은 함수로 호출한다. 따라서 DX12 draw command에 core의 `-8` 특수값이 들어오면 이 분기는 재설정으로 인식하지 않고 그 값을 함수로 호출하려는 코드 경로다.

기준 트리의 cpp·h·inc 전체에서 `AddCallback(` 표기는 `imgui_draw.cpp`의 정의와 `imgui.h`의 선언뿐이며 호출 표기는 찾지 못했다. `ImDrawCallback_ResetRenderState` 직접 사용도 DX11 비교, 선언과 backend 주석 외에는 찾지 못했다. `Framework` 밖의 cpp·h·inc에서는 `UserCallback` 대입, `CmdBuffer`, `ImDrawCmd` 표기도 찾지 못했다. 외부 코드·함수 포인터를 통한 간접 생성 여부는 이 검색으로 배제하지 못하며 DX12 실행도 하지 않았다. 이관 회귀 조건은 DX11·DX12의 특수 callback 처리와 일반 callback, 렌더 상태 재설정이다.

DX12 backend는 렌더 함수의 지역 `render_state` 주소를 `bd->RenderState`에 저장하지만 반환 전에 null로 되돌리는 줄은 주석이다. 이 주소는 함수 반환 뒤 유효한 지역 객체를 가리키지 않는다. 현재 확인한 sampler callback 등록 줄 역시 주석이므로 이 사실만으로 반환 뒤 접근이 발생한다고 판단하지 않는다. callback 공개·수명 변경 시 함께 확인한다.

## DX12 글꼴 atlas 생성과 GPU texture 연결

`Engine.cpp:431-447`은 글꼴을 추가한다. 452행의 `io.Fonts->Build()`는 블록 주석 안에 있어 실행되지 않는다. DX11 backend는 `imgui_impl_dx11.cpp:318-354`에서 `GetTexDataAsRGBA32`로 atlas 픽셀을 만들고 texture·shader resource view를 생성한 뒤 `io.Fonts->SetTexID`를 호출한다.

기준 DX12 backend의 `ImGui_ImplDX12_NewFrame`은 pipeline이 없으면 `CreateDeviceObjects`를 호출하지만 그 함수에는 atlas 픽셀 업로드나 `io.Fonts->SetTexID`가 없다. DX12 파일에 있는 `CreateShaderResourceView`와 `tex->SetTexID` 코드는 `#if 0` 안의 동적 texture 구현이다. 렌더 경로의 동적 업데이트 순회도 주석 처리돼 있다. 자체 cpp·h·inc에서 atlas `SetTexID` 호출은 DX11 backend 외에 활성 코드로 확인되지 않았다. 따라서 현재 소스의 DX12 경로에서는 글꼴 추가 후 CPU atlas를 빌드하거나 GPU texture에 연결하는 활성 경로를 찾지 못했다. `Engine.cpp:564-567`은 DX12 backend의 `NewFrame` 다음 `ImGui::NewFrame`을 호출하며, core `imgui.cpp:9969`는 `Fonts->IsBuilt()`를 assert한다. `IsBuilt`는 글꼴 목록이 있고 atlas `TexReady`가 true여야 한다. atlas 생성자는 메모리를 0으로 초기화하고 `AddFont`는 `TexReady=false`로 설정한다. `ImGui::NewFrame`은 기본 글꼴을 설정하기 전에 이 조건을 확인한다. 따라서 다른 경로가 atlas를 빌드하지 않은 첫 DX12 프레임은 조건을 만족하지 않는다. 실제 assertion 처리와 실행 결과는 빌드 설정과 실행 환경을 확인해야 한다.

이관 회귀 조건은 DX12 atlas 업로드·descriptor 생성, font texture ID 설정, 첫 프레임과 장치 재생성, 한글 glyph 표시다. 실제 컴파일·DX12 화면은 확인하지 않았다.

현재 PC의 12개 구성에 대한 [컴파일 항목 메타데이터](p0/evaluated_all_compile_metadata.csv)에서 `Framework/imgui.cpp`는 Debug에 `_DEBUG`, Release에 `NDEBUG`가 지정된다. 기준 `imgui.h`의 기본 `IM_ASSERT`는 C `assert`이며, 기준 cpp·h·inc에서 별도의 `IM_ASSERT` 또는 `NDEBUG` 재정의는 찾지 못했다. 따라서 이 평가 입력과 기본 assert 규칙이 적용된다면 Debug는 준비되지 않은 atlas 검사를 수행하고 Release는 `NDEBUG`로 해당 검사를 생략한다. 검사를 생략하는 것이 atlas 생성이나 texture 연결을 뜻하지는 않는다. 이는 프로젝트 평가와 소스상의 조건이며 실제 전처리·빌드·실행 결과가 아니다.

기존 core에서 `ImFont` 생성자는 `ContainerAtlas=nullptr`로 초기화하고 atlas 빌드 과정에서 이 포인터를 설정한다. `ImGui::NewFrame`의 준비 검사를 통과하거나 생략한 뒤 `SetCurrentFont`는 기본 글꼴의 `ContainerAtlas`를 읽어 atlas 필드를 사용한다. 따라서 글꼴 추가는 성공했지만 atlas 빌드가 없고 `IM_ASSERT`가 비활성인 경로에서는 null인 atlas를 역참조하는 코드가 이어진다. 이는 소스와 평가된 구성에 근거한 조건부 경로이며 실제 Release 바이너리에서의 결과는 확인하지 않았다.

### 한영 전환 분기와 오른쪽 Alt

`Engine.cpp:99-116`은 UI가 하나라도 열려 있을 때 `WM_KEYDOWN` 또는 `WM_SYSKEYDOWN`의 `wParam`이 `0x15`나 `0xA5`이면 같은 IME 변환 모드 전환 분기로 처리한다. 이 PC Windows SDK의 `WinUser.h`에서 두 상수는 각각 `VK_HANGUL`, `VK_RMENU`다. 분기는 `WantCaptureKeyboard`나 텍스트 입력 활성 여부를 확인하지 않으며, IME 상태 변경을 시도한 후 `DefWindowProc`을 호출하고 반환하므로 아래의 ImGui Win32 키보드 handler에는 전달하지 않는다. 입력 컨텍스트를 얻지 못해도 분기에서 반환한다.

이는 메시지의 `wParam`이 해당 값으로 전달되는 경우의 코드 동작이다. 실제 오른쪽 Alt 메시지 값, 입력기별 모드 변화와 게임의 키 수신 결과는 확인하지 않았다. 이관 회귀 조건에 한영 키와 오른쪽 Alt, 텍스트 입력 활성·비활성, 입력 컨텍스트 부재를 포함한다.

### 메뉴 토글 키의 반복과 전달

`Source_impl.inc`의 MainThread는 `VK_OEM_3`의 눌림 상태를 30ms 간격으로 확인한다. 이전에 누르지 않았던 순간을 따로 검사하지 않고 마지막 요청 후 500ms가 넘으면 메뉴 표시를 켜고 접기·펼치기 요청 bool을 true로 둔다. 따라서 키를 계속 누르고 있으면 500ms를 넘는 간격으로 요청이 반복될 수 있다. `Menu_impl.inc:645`의 렌더 경로는 그 bool을 소비해 마지막 프레임의 접힘 상태를 뒤집는다. 요청은 횟수 큐가 아니라 단일 bool이므로 순차적으로 볼 때 소비 전 여러 번 true를 써도 횟수는 보존되지 않는다.

`MenuState.cpp:145`의 요청 변수는 atomic이 아닌 일반 bool이며 MainThread의 쓰기와 렌더 경로의 읽기·초기화를 묶는 동기화가 이 접근 경로에 없다. 실제 두 호출이 다른 스레드에서 겹치는지는 확인하지 않았고, 위 순차적 설명을 동시 접근 결과의 보장으로 취급하지 않는다. 회귀 조건은 키를 누른 상태 유지, 렌더 지연 중 반복 요청, 메뉴 표시 전환과 스레드 간 전달이다.
