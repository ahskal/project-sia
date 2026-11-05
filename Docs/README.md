# Project Sia

Unreal Engine 기반 C++ 게임 프로젝트입니다.

- 엔진 버전: Unreal Engine 5.8 (`Sia.uproject` 기준)
- 프로젝트 파일: `Sia.uproject`
- 에셋 관리: Git LFS (`.uasset`, `.umap`, `.ubulk`, `.uexp`)

## 시작하기

1. Git LFS를 설치한 뒤 저장소를 복제합니다.
2. `git lfs pull`로 에셋을 내려받습니다.
3. 프로젝트에서 사용하는 엔진 플러그인인 `ModelContextProtocol`, `MCPClientToolset`, `AllToolsets`를 준비합니다.
4. `Sia.uproject`에서 Visual Studio 프로젝트 파일을 생성하고 `Development Editor / Win64`로 빌드합니다.

`Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`는 필요할 때 다시 생성됩니다.
