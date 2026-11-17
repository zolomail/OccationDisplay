# OccationDisplay
Scroller and occation animation display on multiple 64x32 led segment displays using ESP32-S3 SuperMini

#TODO:
    1: On the webUi make it so when selecting a scene to show the user should be able to configure the parameters:
        Scroll text(s)
        Scroll position and speed
        Colors
        Fonts
        etc.

    2: Add user configurable scenes (replace selectDateScene ) the following should be specified:
        which scene
        start: month, day, hour
        end:   month, day, hour
        input: scroll or bounce texts ("Merry Christmas", "Happy Newyear", )
                Year of birth, for calculating age for birthdays
                Colors / fonts

    3: More effects:
        Animated sprites (fish, birds)
        Christmas tree

    4: More Scenes
        Full screen weather
        Easter
        Saint Patrics
        April fools
        Sankt Hans
        Mortens Aften
        Wedding days
        Is it friday?
        Daily/Weekly Reminder (weekend/bedtime)?

    5: Custom scenes
    6: Show events from calendar (eg google calendar)
    7: Add moon cycly to weather icon (at night)

    8: When not being able to connect to WIfi show QR code for the WIFi API ESP32 provides..and the IP to connect to
    9: Move x,y alignment into Effect.h and make enum to define magic numbers eg 0x7FF0 .. 0x7FFF for XPOS_CENTER, XPOS_1_QUART XPOS_3_QUART XPOS_RIGHT (width of display -1)
