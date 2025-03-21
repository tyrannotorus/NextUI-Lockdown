# NextUI Lockdown
NextUI Lockdown aims to support all the goodness of NextUI (for the TriumUI Brick), but with the ability to quickly enable Simple Mode with the konami code.
Useful for when:
1. You want an even more bare-bones minui look, but still want options/settings available when needed.
2. You hand your retro doodad to your nephew to play at Thanksgiving and don't want him messing with settings on accident.
3. When you gift a retro doodad to your technically-illiterate uncle who's melted his brain with Fox News over the last 10 years and he's fully replaced you and all interaction with his family with russian bots on facebook and he can no longer recall joy in his life, including when you two played Super Mario together when you were a kid and you loved your uncle and you just want to give him that little bit of human joy back and go "Hey, do you remember Super Mario? Remember the fun we had? Remember?" Uh, purely hypothetically speaking on this one.

# In Progress : Simple Mode.
  Konami Code enterable at the version screen to enable/disable Simple Mode. Simple Mode should:
  - [x] Hide Tools folder 
  - [x] Hide Favourites
  - [x] Hide Collections
  - [ ] Hide Settings in Emulator menu

![Screenshot from 2025-03-19 15-13-43](https://github.com/user-attachments/assets/f73dfbd3-8b54-4a01-ac9d-55f41cf9d001)
    
# TODO : Single Game Mode.
  Single Game Mode turns your Trimui Brick into a single game retro doodad.
  - [ ] Konami Code in emulator menu of your selected game enables single game mode.
  - [ ] In-game emulator menu has no options in lockdown mode. Just a pause screen.
  - [ ] Device boots directly into game when device is powered on.
  - [ ] Konami Code again in the emulator menu disables single game mode and restores all access to minui.
 
Original NextUI Readme Below.


---


# NextUI (formerly MinUI Next)
A CFW based of MinUI with a rebuild emulation engine and tons of added features for the TrimUI Brick and Smart Pro. For other devices we recommend checking out [MinUI](https://github.com/shauninman/MinUI)
- Fixed both screen tearing and sync stutter problems of MinUI by rebuilding the emulator engine core
- Game switcher menu (Onion OS style) by [@frysee](https://github.com/frysee)
- High audio quality, due to advanced resampling engine using [libsamplerate](https://github.com/libsndfile/libsamplerate) (with quality/performance setting per emulator)
- Much lower latency (average 20ms, 1 frame of 60fps)
- Game art/media support
- Game time tracker!
- Custom bootlogos contributed by [@SolvalouArt](https://bsky.app/profile/solvalouart.bsky.social)
- Color and font settings to customize the NextUI interface
- Menu animations
- LED control, change colors, effects, brightness
- LED indicators, low battery, stand by, charging (brightness can be set seperately)
- Ambient LED mode, the LEDS act like Ambient light TV's for extra immersion, configurable per emulator
- Screen color temperature and brightness controls
- Deep Sleep mode, gives instant ON and avoids the overheat bug on the Brick by [@zhaofengli](https://github.com/zhaofengli)
- Battery Monitoring including history graph and time left prediction
- Scrolling animated titles for longer game names
- Updated and optimized build emulation cores
- Rumble strength fixed and is now variable as used by the games
- FBNeo Arcade screen rotation
- Next font supports CJK for JP/CN etc rom names
- Lot of other smaller fixes and optimizations

Current supported devices:   
Trimui Brick | MinUI-20241028-0   
Trimui Smart Pro | MinUI-20231111b-2 

# Future plans roadmap
- Pal rom mode
- Bluetooth and wifi integrated
- Configurable FN and switch buttons
- More compatibility testing with different emulators and fix/improve if nessecary
- Retroachievements
- Shaders
- Probably we think of a lot of other stuff a long the way to add :D
- Once everything is to my liking I will look into porting this to more devices
- Clean up all MinUI code and strip it from all stuff for legacy devices etc. 

# Super awesome contributions thank you's!
- [@frysee](https://github.com/frysee) for the super awesome game switcher PR. Truly love this feature, thank you so much for contributing it to this project!
  
# Installing   

Just copy MinUI.zip (don't unzip this just copy as zipfile) and trimui to the root of your SD Card, bootup your Trim UI device it will say installing/updating and after that just enjoy all your beloved games!

# How to use
I think most speaks for itself but here are some handy short instructions just in case:
   
While in menu:   
- Hold start and use volume up and down to adjust brightes
- Hold select and use volume up and down to adjust color temperature
- Short press select to open game switcher menu
- Idk the rest speaks for itself I guess?
   
While in game:   
- Menu opens in game options menu, adjusting controls, scaling and what not
- Hold menu and select at same time to open up game switcher
    
Deep sleep:   
- First the device goes into light sleep mode the screen turns off and the Leds will pulse 5 times to let you know its in light sleep, after 2 minutes the device will go in full deep sleep and the leds will also turn completely off. 

Discord:   
[https://discord.gg/HKd7wqZk3h](https://discord.gg/HKd7wqZk3h)


## Watch this awesome review by Play Retro
[![Watch this awesome review by Play Retro](https://img.youtube.com/vi/m_7ObMHAFK4/maxresdefault.jpg)](https://www.youtube.com/watch?v=m_7ObMHAFK4)

## Media
![gameswitcher](https://github.com/user-attachments/assets/4c71dc26-d071-48cf-836e-83bd9a248a32)
![battery](https://github.com/user-attachments/assets/5f8a6f85-7bb7-41b0-95ab-468229a7f443)
![battery](https://github.com/user-attachments/assets/9e7c14b3-757d-4e01-b381-71897e6dc4e2)
![ingamesettings](https://github.com/user-attachments/assets/73fbed30-7aaa-420b-bb53-74dd50160434)
![gamelist](https://github.com/user-attachments/assets/ed0d2552-04c1-40a3-9eb2-14406e83b09a)
![gameoverlay](https://github.com/user-attachments/assets/a7c99784-fa48-4d3e-a64b-28e7149d929a)

# MinUI Readme:
NextUI is based of [MinUI](https://github.com/shauninman/MinUI) which is an amazing CFW and works on many more devices than NextUI currently does, so def check it out!

## Features

- Simple launcher, simple SD card
- No settings or configuration
- No boxart, themes, or distractions
- Automatically hides hidden files
  and extension and region/version 
  cruft in display names

- Consistent in-emulator menu with
  quick access to save states, disc
  changing, and emulator options
- Automatically sleeps after 30 seconds 
  or press POWER to sleep (and wake)
- Automatically powers off while asleep
  after two minutes or hold POWER for
  one second
- Automatically resumes right where
  you left off if powered off while
  in-game, manually or while asleep
- Resume from manually created, last 
  used save state by pressing X in 
  the launcher instead of A
- Streamlined emulator frontend 
  (minarch + libretro cores)
- Single SD card compatible with
  multiple devices from different
  manufacturers

## Supported consoles

Base:

- Game Boy
- Game Boy Color
- Game Boy Advance
- Nintendo Entertainment System
- Super Nintendo Entertainment System
- Sega Genesis
- PlayStation

Extras:

- Neo Geo Pocket (and Color)
- Pico-8
- Pokémon mini
- Sega Game Gear
- Sega Master System
- Super Game Boy
- TurboGrafx-16 (and TurboGrafx-CD)
- Virtual Boy
- Arcade (cps, mame etc)
   
