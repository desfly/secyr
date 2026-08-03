# First firmware through GitHub Actions

## Requirements on Windows

- Git;
- GitHub account;
- GitHub CLI for artifact download;
- no local ESP-IDF required for cloud build.

Authenticate:

```powershell
gh auth login
```

Download successful firmware:

```powershell
.\tools\windows\download-github-artifact-build0023.ps1 `
    -Repository owner/repository
```

Download failure diagnostics:

```powershell
.\tools\windows\download-github-diagnostics-build0023.ps1 `
    -Repository owner/repository
```

Do not flash until the artifact contains:

```text
bootloader.bin
partition-table.bin
homeguard_s3.bin
flasher_args.json
flash_args
manifest.json
SHA256SUMS.txt
```
