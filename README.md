# OccationDisplay
Scroller and occation animation display on multiple 64x32 led segment displays using ESP32-S3 SuperMini

To upload use:
    pio run -t upload

To upload data files:
    pio run -t uploadfs


#TODO:
    2:  Add user configurable scenes (replace selectDateScene ) the following should be specified:
        which scene
        start: month, day, hour
        end:   month, day, hour
        input: scroll or bounce texts ("Merry Christmas", "Happy Newyear", )
                Year of birth, for calculating age for birthdays
                Colors / fonts
    5:  Custom scenes where you can select one background effect (clearScreen, flames, wavesim, Bumpmap ) and up to 5 additional (other effects)
    15: Daily/Weekly Reminders (weekend/bedtime)?
    6: Show events from calendar (eg google calendar)


    3: More effects:
        Animated sprites (fish, birds)
        Christmas tree
        Stars warpspace

    4: More Scenes
        Full screen weather
        Easter
        Saint Patrics
        April fools
        Sankt Hans
        Mortens Aften
        Wedding days
        Is it friday?

    15: Add configuration for all of the effects 
    9: Allow setting outline color for for Hearts and Bubbles 

    11: Dirty rectangel updating...(Fullscreen update every frame max out cpu)
    14: Make wifi retry connect after fail every 30 secs if a valid ssid / pwd exists

    15: Show size of display in webUI
