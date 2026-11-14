# Bundled fonts

Load fonts locally through QML FontLoader; runtime never downloads anything.
`fetch_fonts.sh` restores the files below on a machine with network access.

| File | Design use | Weights | License / Git |
|---|---|---|---|
| BigShouldersDisplay.ttf | Brain display | variable, 500–800 in design | SIL OFL, committed with bigshouldersdisplay-OFL.txt |
| Archivo.ttf | optional expanded display | variable weight and width | SIL OFL, committed with archivo-OFL.txt |
| IBMPlexMono-Regular.ttf | Brain data | 400 | SIL OFL, committed |
| IBMPlexMono-Medium.ttf | Brain data | 500 | SIL OFL, committed |
| IBMPlexMono-SemiBold.ttf | Brain labels | 600 | SIL OFL, committed |
| IBMPlexMono-Bold.ttf | Brain emphasis | 700 | SIL OFL, committed; all four use ibmplexmono-OFL.txt |
| JetBrainsMono.ttf | OS metadata | variable 400/500/700 | SIL OFL, committed with jetbrainsmono-OFL.txt |
| GeneralSans.ttf | GroovePilot OS | variable 400/500/600 | ITF Free Font License, local download only |

General Sans embedding in the app is allowed. **Never commit GeneralSans.ttf or
fonts/general-sans/**. Keep `.gitignore`; the fetcher retains the Fontshare archive
and its license under that ignored directory. If absent (e.g. CI), use the system
sans font and skip its load test. Other missing fonts also fall back gracefully.

The Plex files share the IBM Plex Mono family and identify their real static
weights. QML `Font.Normal/Medium/DemiBold/Bold` select 400/500/600/700 respectively;
loading all four avoids synthetic bold. Display uses Big Shoulders ExtraBold 800;
Archivo expanded may use `font.variableAxes: {"wdth": 125}` when selected.

Sources used by the fetcher: [Google Fonts](https://github.com/google/fonts/tree/main/ofl)
and [Fontshare General Sans](https://www.fontshare.com/fonts/general-sans).
