# P0 빌드 입력 목록

기준: 068ca1d881edf870f7cf06f935cf50e286004af8. 프로젝트 XML에 선언된 항목이며 MSBuild 평가 결과는 아니다. 조건부 제외는 원문을 따른다.

## 컴파일 항목

- `BattleMonitor.cpp`
- `Cheats\Civilian\BangmokCity.cpp`
- `Cheats\Civilian\JewelSettings.cpp`
- `Cheats\Civilian\MissionCpuHeroExclusion.cpp`
- `Cheats\Civilian\MissionAppointmentLimit.cpp`
- `Cheats\Civilian\DomesticRewardCondition.cpp`
- `Cheats\Civilian\DomesticRewardConditionUi.cpp`
- `Cheats\Civilian\TechCityEditorVisibility.cpp`
- `Cheats\Civilian\TechCityEditorVisibilityUi.cpp`
- `Cheats\War\BattleEnvironment.cpp`
- `Cheats\War\BattleMapShuffle.cpp`
- `Cheats\War\Battleunitcapture.cpp`
- `Cheats\Civilian\Bigcityconvert.cpp`
- `Cheats\Officer\RoninMonitor.cpp`
- `Cheats\System\SpeedHack.cpp`
- `Cheats\War\Catapult.cpp`
- `Cheats\War\Celestia.cpp`
- `Cheats.cpp`
- `Cheats\Civilian\CityInfoWindow.cpp`
- `Cheats\Civilian\TavernMonitor.cpp`
- `Cheats\Civilian\DomesticsMult.cpp`
- `Config.cpp`
- `debug.cpp`
- `Cheats\War\Defbuildingboost.cpp`
- `Cheats\War\AIHighHonorSurrenderFix.cpp`
- `Cheats\War\AIExecutionConditionChange.cpp`
- `Cheats\War\AIRefusalWarFix.cpp`
- `Cheats\War\DeputyCaptureFix.cpp`
- `Cheats\War\IsolatedCityCaptureFix.cpp`
- `Cheats\War\PrisonerCaptureManagement.cpp`
- `dllmain.cpp` (조건부 제외: '$(ProxyType)'!='' and '$(ProxyType)'!='dinput8')
- `dllmain_dxgi.cpp` (조건부 제외: '$(ProxyType)'!='dxgi')
- `dllmain_hid.cpp` (조건부 제외: '$(ProxyType)'!='hid')
- `Engine.cpp`
- `Cheats\Social\Fastrelationship.cpp`
- `Cheats\Social\ChildEarlyAppearance.cpp`
- `Cheats\Social\PregnancyManager.cpp`
- `Framework\imgui.cpp`
- `Framework\imgui_demo.cpp`
- `Framework\imgui_draw.cpp`
- `Framework\imgui_impl_dx11.cpp`
- `Framework\imgui_impl_dx12.cpp`
- `Framework\imgui_impl_win32.cpp`
- `Framework\imgui_tables.cpp`
- `Framework\imgui_widgets.cpp`
- `Hooking\buffer.c`
- `Hooking\hde32.c`
- `Hooking\hde64.c`
- `Hooking\hook.c`
- `Hooking\hookmain.cpp`
- `Hooking\trampoline.c`
- `Cheats\Social\Infinitegift.cpp`
- `Cheats\Social\Infinitetalk.cpp`
- `Cheats\Social\InstantLoveCave.cpp`
- `Cheats\Social\Loyaltycave.cpp`
- `Cheats\Civilian\NonggyeongCity.cpp`
- `Cheats\Civilian\SangeopCity.cpp`
- `MemoryUtils.cpp`
- `Menu.cpp`
- `MenuState.cpp`
- `MenuSectionsDomestic.cpp`
- `MenuSectionsCouncil.cpp`
- `MenuSectionsSocial.cpp`
- `MenuSectionsWar.cpp`
- `MenuSectionsOfficerEdit.cpp`
- `MenuSectionsScenario.cpp`
- `MenuSectionsInfo.cpp`
- `MainColumn1.cpp`
- `MainColumn2.cpp`
- `MainColumn3.cpp`
- `Cheats\Officer\OfficerData.cpp`
- `Cheats\Officer\SpecialAbilityAutoAssign.cpp`
- `Cheats\Officer\AIOfficerGrowth.cpp`
- `Cheats\Officer\OfficerDetail.cpp`
- `Cheats\Officer\TraitViewerWidths.cpp`
- `Cheats\Officer\TraitConfigRuntime.cpp`
- `Cheats\Officer\TraitTextEditorData.cpp`
- `Cheats\Officer\TraitTextNameHook.cpp`
- `Cheats\Officer\TraitTextDescHook.cpp`
- `Cheats\Officer\TraitTextSpecialDescHook.cpp`
- `Cheats\Officer\TraitTextEditorWindow.cpp`
- `Cheats\Officer\TraitViewerRoster.cpp`
- `Cheats\Officer\TraitViewerBattleMap.cpp`
- `Cheats\Officer\TraitViewerOfficerInfo.cpp`
- `Cheats\Officer\TraitViewerBattleInfo.cpp`
- `Cheats\Officer\TraitViewerBattlePrep.cpp`
- `Cheats\Officer\TraitViewerFeature.cpp`
- `Cheats\Officer\TraitViewerNativeEditors.cpp`
- `pch.cpp`
- `Cheats\Social\Resonancecave.cpp`
- `Cheats\War\Selfheal.cpp`
- `Cheats\War\StratagemGaugeMax.cpp`
- `Cheats\War\StratagemSlotProbe.cpp`
- `Cheats\War\Spell5HealProbe.cpp`
- `Cheats\War\ShortBattleCooldown.cpp`
- `Cheats\War\TroopCountCombatScaling.cpp`
- `Cheats\War\CouncilExecuteFreeOfficers.cpp`
- `Cheats\War\GovernorPrisonerDisposal.cpp`
- `Cheats\War\ReinforcementArrivalAction.cpp`
- `Cheats\War\ReinforcementDefenderPlacement.cpp`
- `Cheats\War\SiegeWarfare.cpp`
- `Cheats\War\Dongto.cpp`
- `Cheats\Officer\SelectOfficercapture.cpp`
- `Cheats\Officer\SpecialtyInfoWindow.cpp`
- `Cheats\Officer\OfficerRosterResolve.cpp`
- `showcal.cpp`
- `showlog.cpp`
- `Source.cpp`
- `Cheats\War\Terrainignore.cpp`
- `Cheats\Civilian\Techpointcave.cpp`
- `Cheats\Civilian\Techzero.cpp`
- `Cheats\War\Roadblock.cpp`
- `Cheats\System\MonthCapture.cpp`
- `Cheats\System\SystemMonth.cpp`
- `Cheats\System\TengiCave.cpp`
- `Cheats\System\SkillCondition.cpp`
- `Cheats\System\StartSetting.cpp`
- `Cheats\War\FactionLordBonus.cpp`
- `Cheats\Officer\StatMonitor.cpp`
- `Cheats\Officer\BatchOfficerEditor.cpp`
- `NotificationManager.cpp`
- `TacticsEditWindow.cpp`
- `Cheats\System\SkillCountManager.cpp`
- `Cheats\War\SpecialAbility.cpp`
- `Cheats\System\FactionTechEditor.cpp`
- `TextureLoader.cpp`

## 상위 props 추가 항목

- `Cheats\War\RulerTransferProposal.cpp`
- `Cheats\War\RulerTransferProposalBootstrap.cpp`

## 리소스와 조건

- `Resources.rc`: 범위 PNG 11개와 기본 effect/speciality JSON 2개.
- 기본 ProxyType은 hid: `dllmain_hid.cpp` 선택. dinput8과 dxgi 진입점은 조건부. dxgi의 def 지정은 Release x64 설정에만 있다.
- Debug/Release Win32 및 x64 구성 선언, v145, Windows SDK 10.0, Unicode. Release x64에만 C++20, /utf-8, /Zc:char8_t-, /FS가 명시됨.
- 사용자별 Microsoft.Cpp 플랫폼 props를 조건부 import한다. 이 PC에서 평가된 값은 아래 구성 평가를 따르며, 최종 컴파일·링크 명령과 실제 시스템 라이브러리 선택은 빌드하지 않아 확인하지 않았다.

## 현재 환경에서의 MSBuild 평가 (빌드 미실행)

MSBuild의 `-getProperty` 및 `-getItem`만 사용하여 프로젝트를 평가했다. 대상은 Debug/Release x64와 `ProxyType=hid/dinput8/dxgi`의 6개 조합이다. target을 실행하거나 컴파일하지 않았다. [구성별 평가 결과](p0/evaluated_x64_configurations.csv), [컴파일 항목과 제외 여부](p0/evaluated_x64_compile_items.csv)에 결과를 보관한다. 이는 2026-10-31 현재 이 PC의 평가 결과로, 다른 PC나 사용자 props에 적용되는 재현 가능한 잠금값은 아니다.

- 세 프록시 모두 선언된 `ClCompile` 128개 중 진입점 두 개가 제외되어 126개가 남는다. 상위 `Directory.Build.props`의 두 소스가 포함된다. `Resources.rc` 한 개도 평가되었다. `ExcludedFromBuild`는 항목 목록 자체에서 제거하지 않으므로 활성 수는 메타데이터로 구분했다.
- `ProxyType`만 지정했을 때 6개 조합 모두 `TargetName=hid`로 평가된다. 예를 들어 dxgi 또는 dinput8 빌드 시 파일 이름도 바꾸려면 제공된 `build.ps1`처럼 `TargetName`도 지정해야 한다. `ProxyType=dxgi, TargetName=dxgi` 평가에서 `TargetName=dxgi`를 확인했다.
- 일반 컴파일 항목의 Debug x64 값은 `/MDd`에 해당하는 `MultiThreadedDebugDll`, 언어 표준 `Default`, PCH `Use`다. Release x64는 `/MD`에 해당하는 `MultiThreadedDll`, `stdcpp20`, PCH `NotUsing`, `/utf-8 /Zc:char8_t- /FS`다. `pch.cpp`는 두 구성 모두 항목 수준에서 PCH `Create`로 평가된다. 실제 컴파일의 성공 여부는 확인하지 않았다.
- `PlatformToolset=v145`, `WindowsTargetPlatformVersion=10.0`으로 평가된다. `WindowsSDKVersion`은 비어 있지만, 후속 평가에서 실제 SDK 대상에 쓰이는 `TargetPlatformVersion=10.0.26100.0`을 확인했다. 아래 SDK 선택 속성 대조를 따른다.
- 이 사용자 환경은 vcpkg 통합이 활성화되어 `x64-windows` triplet과 사용자 vcpkg include 경로가 컴파일·리소스 메타데이터에 추가되었다. 프로젝트 선언만으로 이 환경 의존성을 재현할 수 없다.
- `dxgi.def`는 프로젝트 XML상 Release x64의 dxgi 구성에서 지정된다. 위 항목 평가에서는 링크 item 정의를 별도로 추출하지 않았고, 실제 링크 명령·DLL export는 확인하지 않았다.

## 로컬 입력 존재와 PCH 추가 확인

기록된 x64 Debug/Release 및 세 proxy 구성의 평가 결과에서 `Excluded`가 `true`가 아닌 컴파일 항목을 프로젝트 디렉터리 기준으로 확인했다. 각 구성의 활성 소스는 모두 로컬 파일로 존재한다. 이 확인은 파일 내용, 전처리 include, 라이브러리, 리소스 또는 컴파일 성공을 보증하지 않는다.

`Directory.Build.props`가 추가하는 `RulerTransferProposalBootstrap.cpp`는 `RulerTransferProposal.h`만 포함하고, 그 헤더에도 `pch.h`가 없다. Debug x64 평가에서 이 항목의 `PrecompiledHeader`는 `Use`이며 프로젝트의 PCH 이름은 `pch.h`다. 프로젝트 XML에는 별도 강제 include 지정이 보이지 않는다. 따라서 평가 시 적용된 다른 속성이나 실제 컴파일 명령이 이 차이를 해소하는지 확인해야 하는 PCH 불일치 후보다. `RulerTransferProposal.cpp`는 직접 `pch.h`를 포함하지 않지만 포함하는 `Config.h`가 이를 포함하므로 두 파일을 같은 사례로 취급하지 않는다. 빌드 금지 지시에 따라 컴파일 오류 발생 여부는 확인하지 않았다.

### PCH 항목 평가 재확인

Debug x64 hid에 대해 MSBuild `-getItem:ClCompile`로 항목 메타데이터를 다시 읽었다. `RulerTransferProposalBootstrap.cpp`, `Framework/imgui.cpp`, `Hooking/buffer.c` 모두 `PrecompiledHeader=Use`, `PrecompiledHeaderFile=pch.h`, `ForcedIncludeFiles` 빈 값, `CompileAs=Default`, `AdditionalOptions` 빈 값이다. 대상 실행 없이 프로젝트 평가만 했다. Bootstrap이 포함하는 자체 헤더에는 다른 include가 없으며, `buffer.c`는 Windows 헤더와 자체 `buffer.h`를 포함한다. `imgui.cpp`의 직접 include와 자체 헤더도 확인했으나 조건부 사용자 설정 include가 있어 모든 전처리 경로를 확정한 것은 아니다. 세 항목은 공통 PCH 설정과 실제 include의 관계를 빌드 단계에서 확인할 후보로 기록한다.

활성 Debug hid 항목 중 `pch.h` 직접 include가 텍스트에 없는 파일은 25개다. 그 안에는 `Config.h`처럼 간접으로 포함하는 경우와 외부 C/C++ 소스가 섞여 있다. 직접 include 검색만으로 25개가 모두 빌드 실패한다고 결론 내리지 않는다. MSBuild 평가는 컴파일러 명령을 생성·실행하지 않았으며 Debug와 Release의 실제 빌드 결과는 여전히 미확인이다.

## 링크 선언과 평가 범위

프로젝트의 네 구성은 모두 Windows subsystem DLL이다. XML상 Debug x64, Debug Win32, Release Win32는 링크 디버그 정보를 켜고 Release x64는 끈다. Release 두 구성은 COMDAT folding과 참조 최적화를 켠다. `dxgi.def`는 Release x64에서 `ProxyType=dxgi`인 경우에만 `ModuleDefinitionFile`로 명시되며, 세 `CreateDXGIFactory*` 이름을 `Proxy_` 함수에 매핑한다. 다른 구성에서 그 세 이름이 어떻게 내보내지는지는 실제 링크 결과를 확인하지 않았다.

`-getItem:Link`는 Release x64 dxgi 평가에서 빈 배열을 반환했다. `Link`는 프로젝트에 독립 항목으로 선언된 것이 아니라 item definition이므로 이 빈 결과를 링크 설정 부재의 근거로 해석하지 않는다. 아래의 후속 평가에서 `ItemDefinitions`를 직접 읽어 환경이 추가한 링크 메타데이터를 확인했다. 실제 최종 링크 명령과 결과는 미확인이다.

소스의 `#pragma comment(lib, ...)` 텍스트 검색에서는 `winmm`, `d3dcompiler`, `gdi32`, `dwmapi`, `user32`, `kernel32`, `shell32`, `imm32`, `Shcore`, `d3d11`, `d3d12`, `dxgi`, `XInput`, `ws2_32`, `wininet`, `Windowscodecs` 선언을 확인했다. 각 선언의 조건부 전처리 및 해당 translation unit의 실제 포함 여부에 따라 활성 라이브러리는 달라지므로 이 목록은 후보이며 최종 링크 목록이 아니다. 솔루션은 x64와 x86 구성을 명시하지만 `build.ps1`은 Release와 proxy 이름만 지정하고 Platform을 지정하지 않는다. 현재 환경에서 솔루션을 `ProjectGraph`로 평가한 기본 플랫폼은 뒤의 솔루션 기반 평가에 기록한다. 스크립트 실행과 실제 빌드 결과는 확인하지 않았다.

## Win32 구성 평가 (빌드 미실행)

현재 PC에서 `-getProperty`와 `-getItem`으로 Debug/Release Win32와 세 proxy의 여섯 조합을 평가했다. [평가 결과](p0/evaluated_win32_configurations.csv)에 저장했다. 각 조합은 컴파일 선언 128개 중 126개가 활성이고 리소스 컴파일 항목 하나가 있다. proxy에 따라 진입점은 hid의 `dllmain_hid.cpp`, dinput8의 `dllmain.cpp`, dxgi의 `dllmain_dxgi.cpp`로 각각 하나다.

일반 `BattleMonitor.cpp` 기준 PCH는 양 구성 모두 `Use`, 언어 표준은 `Default`, CRT는 Debug `/MDd`, Release `/MD`에 대응한다. 현재 사용자 환경의 vcpkg 통합은 `x86-windows`로 평가된다. 세 proxy 모두 출력 이름은 `hid`이고 중간 디렉터리도 같은 구성 안에서 proxy별 구분이 없다. 프로젝트 XML의 `dxgi.def` 지정은 Release x64에만 있으므로 Win32 dxgi 구성에는 해당 명시 지정이 없다. Win32 Release는 x64 Release와 달리 C++20 및 `/utf-8 /Zc:char8_t- /FS`를 프로젝트에서 명시하지 않는다.

이는 구성 평가이지 Win32 빌드나 런타임 지원의 증거가 아니다. 기존 게임 코드에는 64비트 포인터·명령어를 전제로 한 구현이 있으므로 Win32 대상으로 실제 기능을 제공할 수 있는지 별도로 결정하고 검증해야 한다.

### 플랫폼 생략 시 평가의 한계

현재 MSBuild에서 솔루션 파일에 `-getProperty`를 사용해 `build.ps1`의 플랫폼을 확인하려 했으나 `MSB1063`으로 거부됐다. 별도 프로젝트 파일을 `Configuration=Release`, `ProxyType=dxgi`, `TargetName=dxgi`만 지정해 평가하면 현재 환경에서는 `Platform=Win32`, `TargetName=dxgi`, `IntDir=hid\Release\`다. 이 프로젝트 직접 평가 결과를 솔루션 스크립트의 실제 플랫폼으로 대체할 수는 없다. 솔루션 파일은 x86을 프로젝트 Win32에 매핑한다. `Platform` 환경 변수는 이번 확인 시 비어 있었다.

스크립트는 솔루션 경로를 스크립트 위치 기준으로 변환하지 않고 상대 경로 `SAM8RPK_Ingame_Cheat.sln`을 사용한다. 따라서 호출 당시 작업 디렉터리에 따라 솔루션을 찾는 경로가 달라진다. 스크립트 실행이나 빌드는 하지 않았다.

## 미해결 include 후보의 조건

`p0/include_inputs.csv`의 미해결 quoted include 다섯 항목을 직접 확인했다. 파일이 없다는 텍스트 검색 결과만으로 기본 구성의 필수 누락으로 판정하지 않는다.

| 후보 | 소스상 조건 |
|---|---|
| `imgui_user.inl` | `imgui.cpp:16207`의 `IMGUI_INCLUDE_IMGUI_USER_INL`이 정의될 때 |
| `imgui_user.h` | `imgui.h:3431`의 `IMGUI_INCLUDE_IMGUI_USER_H`가 정의되고 별도 파일명 매크로가 없을 때 |
| `stb_sprintf.h` | `imgui.cpp:2003`의 `IMGUI_USE_STB_SPRINTF`가 정의되고 별도 파일명 매크로가 없을 때 |
| `misc/freetype/imgui_freetype.h` | `imgui_draw.cpp:37`의 `IMGUI_ENABLE_FREETYPE`이 정의될 때 |
| `stb_truetype.h` | `imstb_truetype.h:284,335`의 두 예제 모두 `#if 0` 아래 |

기준 프로젝트 디렉터리의 cpp·h·props·vcxproj에서 앞의 네 활성화 매크로를 정의한 전처리 지시문 또는 프로젝트 `PreprocessorDefinitions` 텍스트는 찾지 못했다. 사용자 속성이나 컴파일 명령의 정의, 다른 포함 파일을 통한 정의는 이 검색만으로 배제하지 않는다. 컴파일·전처리는 실행하지 않았다.

## HDE 입력과 플랫폼 분기

프로젝트는 두 `hde32.c`와 `hde64.c`를 모두 `ClCompile`로 선언한다. `hde32.c:8`의 구현은 `_M_IX86` 또는 `__i386__`, `hde64.c:8`의 구현은 `_M_X64` 또는 `__x86_64__`에서만 활성화된다. `trampoline.c:35`는 x64 매크로가 있으면 `hde64.h`와 64비트 disassembler를, 그렇지 않으면 `hde32.h`와 32비트 disassembler를 선택한다. 따라서 두 파일이 빌드 항목으로 평가된 사실과 두 구현이 같은 구성에서 활성화된다는 주장은 구분한다.

이는 소스 조건의 정적 확인이다. 프로젝트가 Win32 구성을 선언한다는 사실은 전체 게임 기능의 32비트 호환성을 증명하지 않는다. Debug 구성의 공통 PCH와 C 소스의 관계, 실제 컴파일러 사전 정의 및 링크 결과는 빌드하지 않아 확인하지 않았다.

### 솔루션 플랫폼 기본값의 추가 확인

위 프로젝트 직접 평가의 Win32 결과와 별도로, 이 PC의 Visual Studio 18 MSBuild `Microsoft.Build.dll`에서 `SolutionFile.Parse`로 실제 솔루션을 읽었다. `GetDefaultPlatformName()`은 `x64`, `GetDefaultConfigurationName()`은 `Debug`를 반환했다. 이어 `ProjectGraph`를 솔루션 경로와 `Configuration=Release`, `ProxyType` 및 `TargetName`만 지정해 생성하여 평가한 프로젝트 값을 확인했다. `Platform` 환경 변수는 비어 있었다.

| `build.ps1`에 대응하는 Type | 평가된 플랫폼 | 중간 디렉터리 | 대상 DLL |
|---|---|---|---|
| dinput8 | x64 | `x64\Release\dinput8\` | `x64\Release\dinput8.dll` |
| dxgi | x64 | `x64\Release\dxgi\` | `x64\Release\dxgi.dll` |
| hid | x64 | `x64\Release\hid\` | `x64\Release\hid.dll` |

이는 솔루션 기반 그래프의 읽기 전용 평가로 프로젝트 파일을 단독 평가했을 때의 Win32 기본값과 다르다. 공식 MSBuild의 [솔루션 그래프 기본 구성 선택 코드](https://github.com/dotnet/msbuild/blob/main/src/Build/Graph/GraphBuilder.cs)도 플랫폼 지정이 없을 때 솔루션 기본 플랫폼을 사용한다. 실제 스크립트는 그래프 빌드 옵션을 지정하지 않으므로 이 결과를 실제 스크립트 실행·빌드 성공의 증거로 사용하지 않는다. 다른 환경에서 `Platform`이 설정되거나 솔루션·MSBuild가 바뀌면 결과도 달라질 수 있다. `build.ps1` 자체와 빌드 target은 실행하지 않았다.

## 빌드 진입 스크립트별 전달값

기준 저장소의 세 스크립트는 모두 솔루션 경로를 현재 작업 디렉터리 기준 상대 경로로 전달하며, `Platform`을 명시하지 않는다. 아래는 스크립트 텍스트 대조이며 실행·빌드 결과가 아니다.

| 진입점 | MSBuild에 넘기는 값 | 확인할 차이 |
|---|---|---|
| `build.bat` | `Configuration=Release` | proxy와 출력 이름을 명령에서 지정하지 않는다. 평가된 기본값은 hid다. MSBuild 탐색 실패와 빌드 실패는 각각 오류 종료 경로가 있다. |
| `build_hid.bat` | `Configuration=Release`, `ProxyType=hid`, `TargetName=hid` | 출력 문구는 `x64\Release\hid.dll`이라고 표시하지만 플랫폼을 인자로 강제하지 않는다. |
| `build.ps1 -Type dinput8/dxgi/hid` | `Configuration=Release`, 각 `ProxyType`, 같은 `TargetName`, `/m` | 세 프록시를 선택할 수 있다. `all`은 dinput8 → dxgi → hid 순서로 실행하고 실패하면 이후 항목을 중단한다. |

세 스크립트의 MSBuild 탐색은 `vswhere` 우선이지만 대체 경로 목록은 동일하지 않다. 프로젝트 자체의 출력 이름 기본값과 솔루션 기본 플랫폼 평가는 앞 절의 별도 근거를 따른다. `build_hid.bat`의 출력 문구만으로 실제 선택 플랫폼이나 산출물이 검증되는 것은 아니다. `build.ps1`은 특정 Type 실패를 출력하고 루프를 계속 벗어나 최종 종료 문구를 출력하는 반면, `all` 실패는 즉시 `return`한다. 이 소스 차이를 실제 호출 측 종료 코드의 확인 결과로 취급하지 않는다.

`build.ps1`의 `Type` 매개변수에는 `ValidateSet`이 있지만 필수 지정이나 기본값은 없다. 인자를 생략하면 코드상 `all` 분기가 아니라 그 외 분기에서 빈 값을 한 항목으로 넘겨 `ProxyType`·`TargetName`을 구성한다. 허용된 네 값 중 하나를 사용한 경우와 구분해야 한다. 스크립트 호출 시 PowerShell과 MSBuild의 실제 빈 속성 처리는 실행하지 않았다.

이관 시 확인 항목은 작업 디렉터리, 솔루션 플랫폼 명시, proxy별 대상 이름·중간 경로·dxgi def, MSBuild 탐색 경로, 실패 전달과 부분 산출물이다. 현재 스크립트와 빌드 target은 실행하지 않았다.

## 현재 PC의 C++ 도구와 설치된 SDK

빌드 target이나 컴파일러를 실행하지 않고 현재 Visual Studio 18 Community의 기본 도구 버전 파일과 설치 경로를 읽었다. 기본 C++ 도구 디렉터리는 `14.51.36231`이며 `bin/Hostx64/x64/cl.exe`와 `lib/x64/vcruntime.lib`가 존재한다. `cl.exe` 파일의 제품 버전 정보는 `14.51.36260.0`이다. 파일의 존재·버전 정보는 컴파일 성공이나 실제 명령행의 증거가 아니다.

hid 프로젝트를 Debug/Release × x64/Win32 네 구성으로 `-getProperty`만 평가했을 때 모두 `VCToolsVersion=14.51.36231`, `WindowsTargetPlatformVersion=10.0`, `WindowsSDKVersion` 빈 값이었다. 이 PC의 Windows Kits 10 include와 lib 디렉터리에는 각각 `10.0.19041.0`, `10.0.22621.0`, `10.0.26100.0` 세 버전이 존재한다. 디렉터리 목록은 설치 후보이며, 후속 속성 평가에서 이 환경의 대상 SDK 버전을 확인했다. 실제 컴파일·링크 명령은 확인하지 않았다. 다른 proxy와 다른 PC의 환경은 이 네 평가만으로 일반화하지 않는다.
### 대상 SDK 버전의 평가 경로

Visual Studio 18의 `Microsoft.Cpp.WindowsSDK.props:113-125`는 프로젝트의 `WindowsTargetPlatformVersion`이 `10.0`이면 설치된 최신 SDK 후보를 찾아 `TargetPlatformVersion`에 설정한다. 이전에 확인한 `WindowsSDKVersion`은 이 프로젝트 평가에서 비어 있어도 `TargetPlatformVersion`과 동일한 속성이 아니다.

hid의 Debug/Release × x64/Win32 네 구성을 다시 `-getProperty`로 평가한 결과 `_LatestWindowsTargetPlatformVersion`과 `TargetPlatformVersion`이 모두 `10.0.26100.0`이었다. 따라서 이 PC의 해당 프로젝트 평가에서 대상 SDK 버전은 `10.0.26100.0`이다. 이 값은 평가 당시 설치 상태와 입력 속성에 의존하며, 실제 컴파일러 명령·링크 결과와 다른 proxy 또는 PC의 설정까지 확인한 것은 아니다. target·컴파일러·빌드는 실행하지 않았다.
### 12개 구성의 대상 SDK와 대표 컴파일 항목 메타데이터

hid·dinput8·dxgi 각각의 Debug/Release × x64/Win32, 총 12개 구성을 빌드 target 없이 다시 평가했다. 각 구성에서 자체 소스 `BattleMonitor.cpp`, 외부 core `Framework/imgui.cpp`, C 소스 `Hooking/buffer.c`, 상위 props 추가 `RulerTransferProposalBootstrap.cpp`의 항목 메타데이터를 [평가표](p0/evaluated_compiler_settings.csv)에 기록했다. 48개 행 모두 활성 항목이며 `TargetPlatformVersion=10.0.26100.0`, `VCToolsVersion=14.51.36231`이다.

대표 네 항목의 프로젝트 전처리 정의는 Debug에 `_DEBUG`, Release에 `NDEBUG`가 있고 Win32 구성에만 `WIN32`가 추가된다. 네 항목에서 강제 include 값은 비어 있다. PCH는 Release x64만 `NotUsing`, 나머지 세 구성은 `Use`이며 언어 표준 메타데이터는 Release x64의 `stdcpp20` 외 `Default`다. include 경로에는 현재 PC의 플랫폼별 vcpkg 경로가 포함된다. 이는 대표 항목의 평가값이고, 컴파일러가 사전 정의하는 매크로·C와 C++에 대한 실제 옵션 해석·최종 명령을 이 표로 증명하지 않는다. PCH 사용과 파일 include의 관계는 앞의 별도 후보 기록을 따른다.
### 전체 컴파일 항목의 평가 메타데이터

같은 12개 구성에서 `ClCompile` 128개씩 전체 1,536개 항목을 평가하고 [전체 메타데이터](p0/evaluated_all_compile_metadata.csv)에 기록했다. 그중 조건부 제외는 24개이며 구성별 활성 항목은 126개다. 모든 항목의 평가된 강제 include 값은 비어 있다. 이 표에는 항목별 전처리 정의, include 경로, PCH, 언어 표준, CompileAs와 추가 옵션을 남겼다.

각 구성에서 `TraitViewerBattleMap.cpp`, `TraitViewerOfficerInfo.cpp`, `TraitViewerBattleInfo.cpp` 세 항목은 공통 정의 외 `NOMINMAX`가 추가된다. `pch.cpp` 한 항목은 PCH `Create`이며 나머지 125개는 Release x64에서 `NotUsing`, 그 밖의 구성에서 `Use`다. 세 `NOMINMAX` 항목도 이 125개에 포함된다. Release x64는 이 활성 126개 항목의 언어 표준 메타데이터가 `stdcpp20`이고 추가 옵션으로 `/utf-8 /Zc:char8_t- /FS`가 평가된다. 나머지 세 구성은 언어 표준이 `Default`다. 이 분류는 MSBuild 항목 메타데이터이며 C 소스에 C++ 표준 옵션이 실제로 전달되는지, 컴파일러 사전 정의와 전처리 결과가 무엇인지는 확인하지 않았다.
### 링커 항목 정의 평가와 vcpkg 후보

MSBuild의 `ProjectCollection.LoadProject`로 프로젝트 평가만 수행하고 `ItemDefinitions["Link"]`의 메타데이터를 12개 구성에서 읽어 [링커 설정표](p0/evaluated_link_settings.csv)에 저장했다. 별도 build target이나 linker는 실행하지 않았다. 12개 모두 Windows subsystem이며 기본 `AdditionalDependencies`에는 kernel32·user32 등 12개 시스템 라이브러리 이름과 현재 PC vcpkg의 플랫폼·구성별 `*.lib` 경로가 들어간다. 소스 `#pragma comment(lib, ...)`가 object에 추가하는 의존성과 최종 linker 명령은 이 평가표에 포함되지 않는다.

`dxgi.def`는 Release x64 dxgi에서만 평가됐다. Debug와 Win32 Release는 링크 디버그 정보 `true`, Release x64는 `false`다. Release의 `LinkTimeCodeGeneration`은 `UseFastLinkTimeCodeGeneration`이고 Debug는 해당 메타데이터가 비어 있다. x64와 Win32의 target machine은 각각 `MachineX64`, `MachineX86`이다. 결과 파일의 export나 실제 LTCG 적용은 확인하지 않았다.

현재 vcpkg x64의 Release·Debug lib 디렉터리에는 각각 `.lib` 42개가 있어 [파일명·크기 후보](p0/vcpkg_link_candidates.csv)에 기록했다. 평가된 Win32의 `x86-windows/lib`와 `x86-windows/debug/lib` 디렉터리는 현재 존재하지 않는다. 와일드카드와 디렉터리의 존재는 각 라이브러리가 실제로 링크된다는 뜻이 아니며, Win32에서 선택한 기능이나 링크가 실패한다고 이 평가만으로 단정하지 않는다. 설정·설치 상태는 PC에 따라 달라진다.
### 소스의 라이브러리 지시문 위치

프로젝트 디렉터리의 cpp·c·h·inc에서 `#pragma comment(lib, ...)` 텍스트 18곳을 찾아 [위치 목록](p0/pragma_link_candidates.csv)에 기록했다. `pch.h`는 D3D11·D3D12·DXGI·XInput·Winsock·WinINet을, 배속·리소스 로더는 각각 winmm·Windowscodecs를 선언한다. Win32와 DX11·DX12 ImGui backend의 지시문도 별도 행으로 남겼다. DX11은 `d3dcompiler`, DX12는 `d3dcompiler.lib`로 표기하며 이 표기는 문자열로 각각 보존했다.

이 지시문에는 전처리 조건과 include 경로가 영향을 준다. 예를 들어 ImGui backend의 D3D compiler 지시문은 `IMGUI_DISABLE`이 정의되지 않고 `_MSC_VER`이 정의된 경우에 있고, Win32 GDI 지시문은 추가로 `NOGDI`가 정의되지 않아야 한다. `pch.h`가 일부 번역 단위에 도달하는지 역시 개별 구성과 PCH 설정에 좌우된다. 따라서 이 목록은 소스 위치 색인이며 앞의 `Link` 평가 메타데이터와 합쳐도 실제 object의 default library 지시나 최종 링크 입력을 증명하지 않는다. 컴파일과 링크는 하지 않았다.
### 리소스 항목과 빌드 이벤트 평가

12개 구성의 `ResourceCompile` 항목과 `PreBuildEvent`, `PreLinkEvent`, `PostBuildEvent` 항목 정의를 프로젝트 평가로 읽어 [구성별 표](p0/evaluated_resource_events.csv)에 기록했다. 모두 활성 `Resources.rc` 한 개이며 리소스 전처리 정의는 `_UNICODE;UNICODE;`, 추가 옵션은 비어 있다. 이 PC의 플랫폼별 vcpkg include 경로가 추가 include 메타데이터에 들어간다. 평가된 `CustomBuild`·`Midl` 항목은 0개, 세 이벤트의 `Command` 메타데이터는 비어 있다.

같은 플랫폼·구성에서는 세 proxy의 `ResourceOutputFileName`도 같다. 예를 들어 x64 Debug는 모두 `hid\x64\Debug\Resources.res`다. 이는 개별 항목과 이벤트 명령의 평가 결과다. 가져온 `.targets`의 다른 target, 실행 중 생성되는 항목, 리소스 컴파일러의 실제 옵션이나 산출물까지 없다는 뜻은 아니다. target·리소스 컴파일러·빌드는 실행하지 않았다.
### 평가된 SDK·도구 검색 경로

현재 PC의 hid Debug/Release × x64/Win32 네 구성에서 `IncludePath`와 `LibraryPath`를 평가해 빈 항목을 제외하고 순서와 디렉터리 존재 여부를 [검색 경로 목록](p0/evaluated_sdk_search_paths.csv)에 기록했다. 각 구성에서 include 경로 9개와 library 경로 6개가 평가됐고 해당 60개 디렉터리는 모두 존재한다. 경로에는 MSVC `14.51.36231`, Windows SDK `10.0.26100.0`의 UCRT·UM 등과 NETFX SDK 4.8 경로가 포함된다. Win32의 Windows SDK 경로 존재와 앞에서 확인한 vcpkg x86 디렉터리 부재는 서로 다른 사실이다.

이 목록은 네 구성의 전역 검색 경로이며 개별 컴파일 항목의 `AdditionalIncludeDirectories`, 링커의 `AdditionalLibraryDirectories` 및 소스 `#pragma` 후보와 함께 봐야 한다. 디렉터리가 있다는 것만으로 필요한 각 헤더·라이브러리가 모두 존재하거나 실제 명령이 이 순서로 사용됐다고 판단하지 않는다. 환경 `PATH` 전체는 저장하지 않았으며 컴파일과 링크는 하지 않았다.
### 시스템 라이브러리 파일 후보 대조

hid의 네 플랫폼·구성에서 평가한 기본 `Link.AdditionalDependencies`의 이름과 [소스 pragma 목록](p0/pragma_link_candidates.csv)의 이름을 합치고, 확장자가 없는 이름에 `.lib`를 붙여 중복 제거했다. 플랫폼·구성마다 24개다. 평가된 `AdditionalLibraryDirectories`와 `LibraryPath`에 같은 이름의 파일이 있는지 읽기 전용으로 확인한 [후보 목록](p0/system_library_file_candidates.csv)은 네 구성 총 96행이며, 각 이름마다 이 검색 범위에 파일이 하나씩 있었다.

이 비교는 이름과 파일 존재만 확인한다. pragma의 활성 조건과 include 도달성, `.lib` 내용·아키텍처, linker가 실제 사용하는 검색 순서, vcpkg `*.lib` 와일드카드의 확장 및 object가 추가할 다른 지시문은 판정하지 않는다. Win32 vcpkg lib 디렉터리 부재는 위 시스템 라이브러리 파일 확인과 별개다. 컴파일·링크는 하지 않았다.
### 시스템 라이브러리 후보의 아키텍처

앞서 이름으로 찾은 라이브러리 96행은 Debug·Release가 같은 경로를 공유해 실제로는 파일 48개다. Visual Studio `dumpbin /headers`로 이 파일들을 읽어 표시되는 COFF machine 종류를 [아키텍처 목록](p0/system_library_architecture.csv)에 저장했다. x64 후보 24개에서는 `x64`, Win32 후보 24개에서는 `x86`만 표시됐고 machine 표기가 없는 파일은 없었다.

이는 확인한 SDK 라이브러리 파일의 헤더 대조다. 심볼 제공 여부, 선택된 pragma의 활성화, vcpkg 와일드카드의 실제 포함, 최종 링커 명령이나 링크 성공을 확인한 것은 아니다. 바이너리는 실행하지 않았고 컴파일과 링크도 하지 않았다.
기준 저장소 `README.md`는 지원 기준을 게임 1.1.3과 Windows x64로 명시하고 Visual Studio에서 x86을 선택하지 말라고 안내한다. 프로젝트 평가에 Win32 구성이 존재한다는 사실과 문서의 지원 범위는 다르다. Win32의 포인터 크기 후보는 선언된 구성의 정적 차이를 보여주며 이 문서가 Win32 지원을 주장하는 근거는 아니다. x64의 실제 컴파일과 게임 동작도 별도 확인이 필요하다.

## 플랫폼별 평가된 컴파일 파일 대조

기존 `p0/evaluated_all_compile_metadata.csv`의 `excluded`가 `true`가 아닌 항목을 기준으로 Debug·Release와 hid·dinput8·dxgi 각각에 대해 x64와 Win32의 `source` 집합을 비교했다. 여섯 쌍 모두 각 플랫폼의 고유 소스 경로가 126개이며 집합 차이는 없었다. 이는 저장된 MSBuild 평가 결과의 경로 문자열 비교다. 파일별 전처리 분기, 포인터 크기, 실제 컴파일과 링크의 동일성을 뜻하지 않는다.

동일 파일 목록이어도 `CityInfoWindow.cpp:910,2200`의 도시 전선 분석과 배치용 전선 판정은 연결 슬롯을 `sizeof(uintptr_t)` 간격으로 읽으므로 x64에서는 8바이트, Win32에서는 4바이트 간격이다. 두 함수 모두 같은 크기의 포인터를 읽는다. 이는 구성별 소스 코드상 차이이며 실제 게임 데이터가 Win32에서 유효하다는 증거는 아니다. 별도의 주점 슬롯 쓰기는 고정 8바이트 간격과 `sizeof(uintptr_t)` 비교 때문에 Win32에서 거부되는 경로가 [기능 보존표](P0_Feature_Contracts.md)에 기록돼 있다.

### 포인터 크기와 아키텍처 조건 후보

[아키텍처 관련 텍스트 후보](p0/architecture_sensitive_candidates.csv)는 기준 자체 cpp·h·inc·c에서 `sizeof(uintptr_t)`, `sizeof(intptr_t)`, `sizeof(void*)`와 지정한 x86/x64 매크로의 전처리 조건을 검색한 99개 위치다. `Framework`와 `Hooking`을 제외했으며, 주석·비활성 코드가 포함될 수 있다. 다른 형식의 포인터 크기 연산, 다른 아키텍처 매크로, 외부 코드와 전처리 결과는 범위 밖이다. 기재 UI의 포인터 배열과 무장 슬롯 주소 계산(`SelectOfficercapture.h:79,90`)도 이 후보에 포함되며 Win32에서는 간격이 4바이트, x64에서는 8바이트다. 이 목록은 플랫폼 호환성 판정이 아니라 추가 대조 위치다.

`Cheats/Civilian/DomesticsMult.cpp:49-95`의 내정 배율 cave 생성은 x64 레지스터를 대상으로 한 명령 바이트를 직접 넣는다. 주소 즉시값도 `uintptr_t`로 기록한 뒤 인덱스를 고정 8바이트 늘린다. Win32에서는 `uintptr_t` 쓰기가 4바이트인데 인덱스는 여전히 8바이트 이동하므로 같은 바이트열 생성이 아니다. 이 파일은 앞의 12개 구성 평가에서 컴파일 입력에 들어가며, 해당 함수의 텍스트에는 x86용 cave를 선택하는 플랫폼 조건이 없다. 이 관찰은 Win32 컴파일 성공이나 실제 패치 실행 결과를 뜻하지 않는다.

같은 포인터 크기 의존 형태는 `TengiCave.cpp:79,98`, `Battleunitcapture.cpp:134-246`, `Resonancecave.cpp:28`에도 있다. 이들은 `uintptr_t`로 주소를 기록한 뒤 출력 위치를 8바이트 이동하고 x64 명령 바이트를 생성한다. Win32 평가에서 포인터 기록 폭과 이동 폭이 다르므로 x64와 같은 코드 바이트가 만들어진다고 취급할 수 없다. 세 소스는 구성별 활성 컴파일 목록에 포함되지만, 실제 분기 도달·컴파일·실행은 확인하지 않았다.

고정 폭 주소 출력의 추가 후보를 [위치 목록](p0/pointer_immediate_candidates.csv)에 정리했다. 자체 cpp·h·inc에서 `uintptr_t` 포인터 역참조로 바이트 배열에 쓰는 줄과 그 뒤 세 줄 안의 `idx` 또는 `cur` 8바이트 증가를 텍스트로 찾은 결과 9개 파일 29곳이다. 호출되지 않는 기술 포인트 캡처 후보와 조건부 코드도 포함하며, 다른 변수명·다른 표현·여러 줄 대입은 빠질 수 있다. 이 목록은 실제 전처리 도달성이나 각 cave의 실행을 증명하지 않는다.

이 후보 중 `Infinitetalk.cpp:68`은 담화 무제한 setter의 cave, `Loyaltycave.cpp:58`은 충성도 setter의 설치 cave, `InstantLoveCave.cpp:67,114,169`은 즉시 경애 설치 worker의 cave에 속한다. 이 setter들은 메뉴·기본 설정 적용 경로가 조사돼 있다([기능 보존표](P0_Feature_Contracts.md)). 반면 `Techpointcave.cpp:45,57`의 setter는 기준 자체 소스에서 직접 호출을 찾지 못했다. 텍스트상 후보가 모두 실제 설치된다는 뜻은 아니며 패턴·설정·게임 상태가 각 설치에 영향을 준다.

`OfficerData_impl.inc:355` 후보는 친밀도 쓰기 진단 cave의 복귀 주소다. 이 include는 `OfficerData.cpp`에서 포함되며 도시 무장 정보의 검증 버튼이 설치를 요청하는 경로가 있다. 해당 진단은 공통 MinHook이 아니라 직접 패치이며 설치 후 해제 수명은 [시작·종료 조사](P0_Lifecycle.md#친밀도-쓰기-진단의-직접-hook-수명)에 기록했다. 이 역시 Win32에서의 실제 빌드·실행 결과는 아니다.

`StratagemSlotProbe.cpp:953,1733,2152,2278,2435,2546`의 여섯 로컬 `e64`는 `uintptr_t` 값의 주소에서 8바이트를 `memcpy`한다. Win32에서 `uintptr_t`가 4바이트이므로 이 함수가 실행되면 매개변수 객체 크기를 넘어 읽는다. 각 함수의 설치 조건·전처리 활성 여부와 Win32 실행은 별도 확인 대상이다. 같은 검색에서 나온 `TraitViewerRoster.cpp:189-195`의 8바이트 복사는 원본 변수가 `uint64_t`이므로 이 크기 불일치 사례로 묶지 않는다. 텍스트 패턴으로 찾은 위치에 한정하며 다른 형태의 고정 크기 복사를 망라하지 않는다.

이 여섯 `e64` 중 1733·2152·2278·2435·2546은 각각 선택 처리·압축 초기화·버튼 배치·callback 사전 처리·callback 반복의 설치 함수에 있다. `PrepareStratagemFiveUiBridge`는 이 다섯 설치 함수를 차례로 호출한다(`StratagemSlotProbe.cpp:4258-4270`). 주변의 `S8RPK_STRATAGEM5_DEEP_DIAGNOSTICS` 조건은 중간의 세 진단 로그 호출만 감싸며 이 다섯 호출을 감싸지 않는다. 조기 시작의 실제 이름 치환을 반영하면 `Source.cpp`의 wrapper가 이 bridge를 호출한다. 각 설치 함수가 대상 확인이나 할당에 실패하면 `e64` 실행까지 도달하지 않을 수 있다. 나머지 953은 UI 버튼 등록 중 `EnsureFifthUiGetTrickButtonHook`에 도달했을 때의 코드다. 실제 전처리·설치 여부는 확인하지 않았다.
