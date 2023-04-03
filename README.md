# NeoCD RX
https://github.com/niuus/NeoCDRX/

**_NeoCD RX_** is a _Neo Geo CD / Neo Geo CDZ_ emulator for the _GameCube_, _Wii_, and _Wii U_'s
Virtual Wii. It owes its existence to various other emulators:
**_NEO-CD REDUX_** (Softdev), **_NeoGeo CD Redux_** (Infact), **_NeoCD-Wii_** (Wiimpathy
/ Jacobeian), **_NeoCD Redux Unofficial_** (megalomaniac). **_NEO-CD REDUX_** was itself
based on **_NeoCD/SDL_** 0.3.1 (Foster) and **_NeoGeo CDZ_** (NJ) emulator,
which are source ports of the original **_NeoCD_** emulator (Fabrice Martinez).

**_NeoCD RX_** is a "homebrew application" which means you will need a way to run
unsigned code on your Nintendo Wii. The best website for getting started with
Wii homebrew is WiiBrew (www.wiibrew.org).

Based/forked from:
https://github.com/emukidid/neogeo-cd-redux

(Under GPL License)


## FEATURES

* Z80 emulator core 3.6
*	M68000 emulator core 3.3
*	Wii Remote, Wii Remote Plus, Wii Remote+Nunchuk, and GameCube controller support
*	SD/SDHC, USB, WKF (GameCube), IDE-EXI V1, DVD support
*	UStealth USB devices support
*	Region select for uncut content and extra languages (USA / Europe / Japan)
*	Neo Geo CD Virtual Memory Card (8KiB battery-backed SRAM chip) support. Save directly to SD/USB or to your physical GameCube card for max nostalgia!
*	Sound FX / Music 3-Band equalizer
*	Super fast loading times. Original console weakness is no more!
* Available in various skins/colors
* Open Source!


## RECENT CHANGELOG

[1.0.02 - April 03, 2023]
* First release.
* Fully working SDHC & USB support. You shouldn't see the emulator complaining again about the BIOS not being found, if you already have the proper file in the correct folder, specially on USB devices.
* Virtual Neo Geo Memory Card saving is fixed again. Use SD/USB or the GameCube Memory Card, the latter also works on Wii (backwards compatible unit).
* Fixed GameCube controller analog stick support. It wasn't working on Wii, only when used on a GameCube console.
* Rearranged internal menus for future expansion.
* Some tidying up.

[older update history in the NeoCDRX_manual.pdf]


## INSTALLATION AND USE

To use NeoCD-RX on the Wii / Wii U's Virtual Wii, you will need to extract the
"**_apps_**" and "**_NeoCDRX_**" folders (directories) from the .zip directly to the root
of your SD or USB media. It comes pre-packaged in the Homebrew Channel format,
also compatible with the official forwarders. Then, you need to place your game
files and music tracks into individually named folders inside the "**_\NeoCDRX\games_**"
directory (an in-depth explanation for this in the correspondent section further below).
For the GameCube port, you only need to take care of the "**_NeoCDRX_**" folder included.

Finally, you need to obtain a proper dump of the _Neo Geo CD/CDZ_ console BIOS.
Copy the file inside the "**_\NeoCDRX\bios_**" directory and name it "**_NeoCD.bin_**".
The emulator only works with the following:

```
Neo Geo CDZ BIOS (NeoCD.bin)
Size: 524.288 bytes
CRC32: DF9DE490
MD5: F39572AF7584CB5B3F70AE8CC848ABA2
SHA-1: 7BB26D1E5D1E930515219CB18BCDE5B7B23E2EDA
```
```
Neo Geo CDZ BIOS (NeoCD.bin)
Size: 524.288 bytes
CRC32: 33697892
MD5: 11526D58D4C524DAEF7D5D677DC6B004
SHA-1: B0F1C4FA8D4492A04431805F6537138B842B549F
```

Once you are done, you can proceed to run the emulator. Additionally, you can
install the NeoCD-RX Forwarder Channel in your _Wii_ or _vWii_ System Menu, or the
special NeoCD-RX Channel for Wii U, which reads the configuration and necessary
files from your device "**_\NeoCDRX_**" folder, be it SD or USB.


## CONFIGURATION

To configure NeoCD-RX, press 'A' on the "Settings" box. This will bring up a
screen where you can configure "Region", "Save Device", and "FX / Music Equalizer".

```
• "Region" will allow you to change the emulated console region, to access other
languages and in some cases, change or uncensor game content (fatalities, blood,
difficulty, lives, title screens, etc.). Reload the game (not reset) for the
setting to take effect.

• "Save Device" offers two options, use "SD/USB" to save the SRAM memory
(sort of a virtual memory card implemented inside the real Neo Geo CD console)
directly to the media drive, or use "MEM Card" to save to a physical GameCube
Memory Card, as you would on a real Neo Geo AES, to take your progress to
another console, or just for the nostalgia factor.

• "FX / Music Equalizer" allows you to raise the volume on sound FX or MP3
tracks, or raise the gain in Low / Mid / High frequencies to your liking.
```


## PREPARING THE GAMES FOR USE WITH THE EMULATOR

For every game disc, you need to create a subdirectory inside the included
"**_\NeoCDRX\games_**" named whatever you like, and copy all the game data files
there. Inside this folder, create another subdirectory called "**_mp3_**", where
you have to copy your music tracks. **IMPORTANT**: even if you won't use the
music, the folder is needed.

The music tracks need to be encoded from the original CD's Red Book standard
44.1 kHz WAV, to MP3 format (128kbps minimum, or better), named exactly
"**_TrackXX.mp3_**" where XX is a number that always starts at 02, as the data
track is always 01. Free CD audio ripping software is readily available.

Examples and pictures are inside the **NeoCDRX_manual.pdf**

After this, you are more than ready to start playing. Each game folder you
make will be treated by the emulator as a full CD.


## SUPPORTED CONTROLLERS

NeoCD RX currently supports the following:

```
	• Wii Remote (horizontal)

	• Wii Remote Plus (or Wii MotionPlus adapter)

	• Wii Remote+Nunchuk

	• GameCube controller
```

## DEFAULT MAPPINGS

### GameCube Controller
			Neo Geo A = B
			Neo Geo B = A
			Neo Geo C = Y
			Neo Geo D = X
			Neo Geo Select = Z
			Neo Geo Start = START
			Neo Geo directions = Dpad or Analog Stick
### Wii Remote (horizontal)
			Neo Geo A = 1
			Neo Geo B = 2
			Neo Geo C = B
			Neo Geo D = A
			Neo Geo Select = MINUS (-)
			Neo Geo Start = PLUS (+)
			Neo Geo directions = Dpad (horizontal)
### Wii Remote+ Nunchuk
			Neo Geo A = A
			Neo Geo B = B
			Neo Geo C = PLUS (+)
			Neo Geo D = 1
			Neo Geo Select = MINUS (-)
			Neo Geo Start = PLUS (+)
			Neo Geo directions = Analog Stick


## EMULATOR MAPPINGS

_Force saving to Virtual Memory Card (while in-game, for the games that
support it)_
```
"R" button (GameCube controller)
"PLUS (+)" and "MINUS (-)" buttons together (Wii Remote / Wii Remote+Nunchuk)
```
_Navigation_
```
Dpad or Left Analog Stick (GameCube controller)
Dpad (Horizontal Wii Remote)
Dpad or Nunchuk Analog Stick (Wii Remote+Nunchuk)
```
_Enter directory or Menu option / Change setting_
```
"A" button (GameCube controller)
Button "2" (Wii Remote / Wii Remote+Nunchuk)
```
_Go back from any Menu_
```
"B" button (GameCube controller)
Button "1" (Wii Remote / Wii Remote+Nunchuk)
```
_Go back from Game List_
```
"Z" button (GameCube controller)
Button "HOME" (Wii Remote / Wii Remote+Nunchuk)
```
_Navigate one page forward on the Game List (when you have more than 8 titles)_
```
"R" button (GameCube controller)
"PLUS (+)" button (Wii Remote / Wii Remote+Nunchuk)
```
_Navigate one page backwards on the Game List (when you have more than 8 titles)_
```
"L" button (GameCube controller)
"MINUS (-)" button (Wii Remote / Wii Remote+Nunchuk)
```
_Mount and run a valid game directory_
```
"A" button (GameCube controller)
Button "2" (Wii Remote / Wii Remote+Nunchuk)
```
_Failsafe video mode (Force Menu to 480i with Component / Digital cable)_
```
Hold "L" button right before the emulator is loading to activate
```


## CREDITS & THANKS
```
• NeoCD-Wii (Wiimpathy / Jacobeian)
• NeoCD Redux Unofficial (megalomaniac)
• NeoGeo CD Redux (Infact)
• NEO-CD REDUX (softdev)
• NeoCD/SDL 0.3.1 (Foster)
• NeoGeo CDZ (NJ)
• NeoCD 0.8 (Fabrice Martinez)
• [M68000 C Core](https://github.com/kstenerud/Musashi) (Karl Stenerud)
• [MAME Z80 C Core](https://github.com/mamedev/mame/tree/master/src/devices/cpu/z80) (Juergen Buchmueller)
• Sound Core (MAMEDev.org)
• The EQ Cookbook (Neil C / Etanza Systems)
• The EQ Cookbook (float only version code - Shagkur)
• WKF & IDE-EXI V1 (code from [Swiss GC](https://github.com/emukidid/swiss-gc) - emu_kidid)
• libMAD (Underbit Technologies)
• libZ (zlib.org)
• TehSkeen forum (2006-2009)
• NeoCDRX emu bg - Style 1 (catar1n0)
• NeoCDRX menu design (NiuuS)
```


## RELEVANT LINKS

* Newest/Latest NeoCDRX release at:
https://github.com/niuus/NeoCDRX/releases
