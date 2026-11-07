// What a panel spot can show. The values are enum Complication in
// src/c/Jarl.c. "requires" options are removed on watches without the
// hardware (see index.js); "top" options only fit the top row.
var COMPLICATIONS = [
  { "label": "Nothing", "value": "0" },
  { "label": "Weather icon", "value": "1", "top": true },
  { "label": "Temperature", "value": "2" },
  { "label": "Today's high", "value": "12" },
  { "label": "Today's low", "value": "13" },
  { "label": "Humidity", "value": "14" },
  { "label": "Chance of rain", "value": "15" },
  { "label": "Sunrise", "value": "16" },
  { "label": "Sunset", "value": "17" },
  { "label": "Day of the week", "value": "3" },
  { "label": "Day of the month", "value": "4" },
  { "label": "Month", "value": "5" },
  { "label": "Week number", "value": "6" },
  { "label": "Day of the year", "value": "7" },
  { "label": "Battery", "value": "8" },
  { "label": "Steps", "value": "9", "requires": "health" },
  { "label": "Distance walked", "value": "10", "requires": "health" },
  { "label": "Heart rate", "value": "11", "requires": "heartrate" }
];

function slot(messageKey, label, defaultValue, top) {
  return {
    "type": "select",
    "messageKey": messageKey,
    "label": label,
    "defaultValue": defaultValue,
    "options": COMPLICATIONS.filter(function(c) { return top || !c.top; })
  };
}

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
        "type": "select",
        "messageKey": "WeatherInterval",
        "label": "Weather update",
        "defaultValue": "30",
        "options": [
          { "label": "Every 30 minutes", "value": "30" },
          { "label": "Every hour", "value": "60" },
          { "label": "Every 2 hours", "value": "120" }
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
    "items": [
      {
        "type": "heading",
        "defaultValue": "Info panel"
      },
      {
        "type": "text",
        "defaultValue": "Shown when you flick your wrist."
      },
      slot("Slot1", "Top left", "2", true),
      slot("Slot2", "Top right", "1", true),
      slot("Slot3", "Middle left", "3", false),
      slot("Slot4", "Middle right", "4", false)
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