# logcat-color README

Add color to logcat logs.

Sometimes we need to copy a chunk of logcat to Code for further analyse, this extension might help.

To activate the extension, press `Ctrl`+`Shift`+`P` to open Command Pallette, and choose "Change Language Mode" -> "Logcat".

## Features

- Color (the default colors are controlled by your color theme)
- Customizable color
  - You can change the color of each log level, in your settings.json, e.g,

  ```json
    "editor.tokenColorCustomizations": {
        "textMateRules": [
            {
                "scope": "markup.other.logcat.verbose",
                "settings": {
                    "foreground": "#BBBBBB"
                }
            },
            {
                "scope": "markup.quote.logcat.info",
                "settings": {
                    "foreground": "#FFFFFF"
                }
            },
            {
                "scope": "markup.changed.logcat.debug",
                "settings": {
                    "foreground": "#23AAFF"
                }
            },
            {
                "scope": "markup.inserted.logcat.warning",
                "settings": {
                    "foreground": "#F0ED00"
                }
            },
            {
                "scope": "markup.deleted.logcat.error",
                "settings": {
                    "foreground": "#FF6B68"
                }
            },
            {
                "scope": "markup.deleted.logcat.assert",
                "settings": {
                    "foreground": "#FF6B68",
                    "fontStyle": "underline"
                }
            },
            {
                "scope": "entity.tag.logcat",
                "settings": {
                    "foreground": "#00E",
                    "fontStyle": "bold underline"
                }
            }
        ]
    }
  ```

  - You can also change the color of some part of log line, with one of `scope`s "entity.date.logcat", "entity.time.logcat", "entity.pid.logcat", "entity.tid.logcat", "entity.package.logcat", "entity.level.logcat", "entity.tag.logcat", "entity.message.logcat"
- Support Android Studio output with different configurations and also raw output from "adb shell logcat"
  - You can disable any part of the logcat in Android Studio
  - You can change the time format to "seconds since epoch" in Android Studio
  - You can also copy logcat output directly from "adb shell logcat"
- Calculate the duration of logs of your selection (inspired by [Log File Highlighter](https://marketplace.visualstudio.com/items?itemName=emilast.LogFileHighlighter))
  - The duration is calcualted from the line of the start of your selection until the end of you selection

![Imgur](https://i.imgur.com/NrHcX1n.jpg)

## Known Issues

## Release Notes

### 0.0.1

Initial release
