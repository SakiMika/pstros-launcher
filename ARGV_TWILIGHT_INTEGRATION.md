# PSTROS + TWiLight Menu++ generic JAR launcher

This tree adds ARGV launch support for any `.jar` filename.

## Runtime behavior

- If PSTROS is started normally (no JAR in argv), the existing recursive FAT JAR chooser remains unchanged.
- If PSTROS is started with a JAR path as `argv[1]`, that exact file is launched directly.
- The JAR name is never hard-coded.
- Paths containing spaces are passed as one argv item by TWiLight Menu++'s custom-launcher system.
- On a normal MIDlet exit in ARGV mode, PSTROS exits instead of opening its internal JAR chooser.

## TWiLight Menu++ files

Copy the contents of `twilight_addon/` to the root of the SD/flashcard containing TWiLight Menu++.
Then place the compiled launcher at:

`/_nds/TWiLightMenu/emulators/pstro_launcher.nds`

The included `config.jar.ini` registers `.jar` as a file type and uses `ARG=%PATH%`, so any selected JAR path is passed to PSTROS.

The included `pstros_java.png` is converted from the supplied Java icon to the TWiLight PNG-banner constraints: 32x32 and 15 colors.

## Files installed

- `/_nds/TWiLightMenu/extras/config.jar.ini`
- `/_nds/TWiLightMenu/extras/pstros_java.png`
- `/_nds/TWiLightMenu/emulators/pstro_launcher.nds` (build output; copy it here)

## Important

TWiLight Menu++ only lists an extension configured by `config.<ext>.ini` when the configured launcher binary exists on the card. Therefore `.jar` will not appear until `pstro_launcher.nds` is in the path above.
