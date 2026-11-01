# 외부 의존성

| 구성 | 출처 | 고정 기준 | 고지 |
|---|---|---|---|
| MinHook 및 포함된 HDE | https://github.com/TsudaKageyu/minhook | v1.3.3, `9fbd087432700d73fc571118d6a9697a36443d88` | [원문 라이선스](minhook/LICENSE.txt) |
| Dear ImGui 및 포함된 stb | 기존 S8RPKCheats `Framework` 17개 파일 | 기준 `068ca1d881edf870f7cf06f935cf50e286004af8`에서 복사. 공식 후보 `0ebf49b4c1fa9256a2ea002245a1186075fe7567`과 동일 원본이라고 확정하지 않음 | [MIT 고지](imgui_legacy/LICENSE.txt), 각 stb 헤더의 원문 고지 |
| JSON for Modern C++ | https://github.com/nlohmann/json | v3.12.0, `55f93686c01528224f448c19128836e7df245f72` | [MIT 고지](nlohmann_json/LICENSE.MIT) |
| NVIDIA stdexec | https://github.com/NVIDIA/stdexec | `314637d32d23a3b22ff4b45aa0b17d53a60be0c2` | [라이선스](stdexec/LICENSE.txt) |
| SAN8R Resource Viewer | 사용자 제공 Python 소스 0.2.1 | `core.py`·`fontcodec.py`의 RDB/RDX·FDATA LZ4·G1N 해석을 C++로 이관 | [MIT 고지](san8r_resource_viewer/LICENSE.txt) |

`minhook/`은 독립 Git 체크아웃이다. 이 프로젝트의 C 전용 [MSBuild 프로젝트](../src/platform/windows/S8.MinHook.vcxproj)가 x64 소스 네 개를 사용한다. 원본 S8RPKCheats에 포함된 파일과의 차이는 [P0 출처 조사](../docs/design/P0_Proxy_Origin.md)에 기록되어 있다. 아직 컴파일하거나 게임에서 검증하지 않았다.

`imgui_legacy/`는 기존 포함 버전의 독립 복사본이며 원본 17개 파일과 SHA-256이 일치한다. `imgui_upstream/`은 비교에 사용한 공식 후보의 별도 체크아웃이다. [ImGui 프로젝트](../src/overlay/S8.ImGui.vcxproj)의 C++23 호환성과 backend 동작은 아직 검증하지 않았다.
