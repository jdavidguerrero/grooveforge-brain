# Optional bundled fonts

No font binaries are included or downloaded by the application. Run `bash fetch_fonts.sh`
manually with network access, then restart. Keep the downloaded licenses alongside fonts.
Missing files select system sans/mono without a failed FontLoader request.

| Family | License | Source | Local filename |
|---|---|---|---|
| Big Shoulders Display | SIL OFL 1.1 | https://fonts.google.com/specimen/Big+Shoulders+Display | BigShouldersDisplay.ttf |
| IBM Plex Mono | SIL OFL 1.1 | https://fonts.google.com/specimen/IBM+Plex+Mono | IBMPlexMono.ttf |
| Archivo | SIL OFL 1.1 | https://fonts.google.com/specimen/Archivo | Archivo.ttf |
| JetBrains Mono | SIL OFL 1.1 | https://fonts.google.com/specimen/JetBrains+Mono | JetBrainsMono.ttf |
| General Sans | Fontshare ITF Free Font License (FFL), **not OFL** | https://www.fontshare.com/fonts/general-sans | GeneralSans.ttf |

General Sans is distributed under Fontshare's license: review the included license
before redistributing. Its download is a ZIP; the script keeps the extracted
license and copies a variable (or regular) font to the filename above. Archivo is the alternate display face in the source design.
