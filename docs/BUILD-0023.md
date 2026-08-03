# HomeGuard-S3 Build-0023

Build-0023 prepares the project for the first real GitHub Actions compile
and makes failed builds diagnosable.

## New

- Windows script to publish the complete source tree to a GitHub repository;
- ESP-IDF source compatibility audit;
- component dependency audit;
- detection of missing headers;
- compile command export;
- build output log capture;
- diagnostic artifact uploaded even after CI failure;
- successful firmware artifact with binaries, manifest and hashes;
- Windows scripts to download firmware or diagnostics through GitHub CLI;
- release and diagnostic artifacts have different names.

## CI artifacts

Successful build:

```text
HomeGuard-S3-Build-0023-firmware
```

Always, including failure:

```text
HomeGuard-S3-Build-0023-diagnostics
```

## Recommended sequence

1. Extract the complete Build-0023 archive.
2. Create an empty GitHub repository.
3. Run:

```powershell
.\tools\windows\publish-build0023-to-github.ps1 `
    -RepositoryUrl https://github.com/<owner>/<repo>.git
```

4. Open GitHub Actions.
5. Run `ESP-IDF Build-0023`.
6. On failure, download diagnostics.
7. On success, download the firmware artifact.
