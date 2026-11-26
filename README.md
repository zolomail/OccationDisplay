# OccationDisplay
Scroller and occation animation display on multiple 64x32 led segment displays using ESP32-S3 SuperMini

To upload use:
    pio run -t upload

To upload data files:
    pio run -t uploadfs

To convert fonts run:
    .\convert_fonts.cmd

To convert pngs to SDF atlasses:
    .\convert_sdf.cmd

#TODO:
    EFFECTS AND OCCATION PLANNING
    - Add user configurable scenes (replace selectDateScene ) the following should be specified:
        which scene
        start: month, day, hour
        end:   month, day, hour
        input: scroll or bounce texts ("Merry Christmas", "Happy Newyear", )
                Year of birth, for calculating age for birthdays
                Colors / fonts
    - Daily/Weekly Reminders (weekend/bedtime)?
    - Add configuration for all of the effects 

    - More effects:
        Animated sprites (fish, birds)
        Christmas tree
        Stars warpspace

    - More Scenes
        Full screen weather
        Easter
        Saint Patrics
        April fools
        Sankt Hans
        Mortens Aften
        Wedding days
        Is it friday?

    - Allow setting outline color for for Hearts and Bubbles 
    - Change "Couldn't connect" etc font so it doesn't spill outside display whehn connect failed
    - Use dual wifi mode and try connecting to wifi when valid ssid and pwd exists
    - Make wifi retry connect after fail every 30 secs if a valid ssid / pwd exists

    - Show events from calendar (eg google calendar)
    - Custom scenes where you can select one background effect (clearScreen, flames, wavesim, Bumpmap ) and up to 5 additional (other effects)
    - Dirty rectangel updating...(Fullscreen update every frame max out cpu)

    

Done:
    - Figure out how we incorporate weather...
    - Full border
    - On the webUi make it so when selecting a scene to show the user should be able to configure the parameters (Scroll text(s), Scroll position and speed, Colors, Fonts, etc.)
    - Add moon cycly to weather icon (at night)
    - When not being able to connect to WIfi show QR code for the WIFi AP ESP32 provides..and the IP to connect to
    - Move x,y alignment into Effect.h and make enum to define magic numbers eg 0x7FF0 .. 0x7FFF for XPOS_CENTER, XPOS_1_QUART XPOS_3_QUART XPOS_RIGHT (width of display -1)
    - Golf player on grass standing on right side and flag and hole on the right side (tracman logo scrolling in background and countdown to beta)
    - Add CPU load debug info bar on right side of display (from bottom to top indicating how loaded the cpu is)
    - Fix snow piling up on the left side of the display
    - WebUi should get font list from enum in Effects.h
    - Move web templates out into filesystem
    - Add countdown argument to showtime so it will show days, hours, minutes and seconds to a specified date (no year)
    - Show size of display in webUI
    - Add a memory bar right next to the cpu bar add a bar that shows memory used vs total
    - Zolomicro missing lowercase
    - Allow calibrating panels individually
    - Add brightness aadjustment in debug
    - Fix "showing new years eve" when no date time
