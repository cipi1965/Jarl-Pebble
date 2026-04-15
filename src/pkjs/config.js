module.exports = [
  {
    "type": "heading",
    "defaultValue": "Jarl Configuration"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Settings"
      },
      {
        "type": "toggle",
        "messageKey": "AnimateNumbers",
        "label": "Animate numbers",
        "defaultValue": true
      },
      {
        "type": "radiogroup",
        "messageKey": "TempUnits",
        "label": "Temperature Unit",
        default: "c",
        "options": [
          { 
            "label": "Fahrenheit", 
            "value": "f"
          },
          { 
            "label": "Celsius", 
            "value": "c" 
          }
        ]
      },
      {
        "type": "toggle",
        "messageKey": "VibrateOnConnect",
        "label": "Vibrate on connect",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "VibrateOnDisconnect",
        "label": "Vibrate on disconnect",
        "defaultValue": false
      },
    ]
  },
  {
    "type": "section",
    "capabilities" : ["COLOR"],
    "items": [
      {
        "type": "heading",
        "defaultValue": "Colors"
      },
      {
        "type": "color",
        "messageKey": "Color1",
        "label": "Color 1",
        "defaultValue": "aa0000",
        "sunlight": true
      },
      {
        "type": "color",
        "messageKey": "Color2",
        "label": "Color 2",
        "defaultValue": "00aaff",
        "sunlight": true
      },
      {
        "type": "color",
        "messageKey": "Color3",
        "label": "Color 3",
        "defaultValue": "ffff00",
        "sunlight": true
      },
      {
        "type": "color",
        "messageKey": "Color4",
        "label": "Color 4",
        "defaultValue": "00ff00",
        "sunlight": true
      },
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];