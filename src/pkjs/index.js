// Import the Clay package
var Clay = require('@rebble/clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay. Events are handled below so the settings page only lists
// the complications this watch supports.
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var HEALTH_PLATFORMS = ['basalt', 'chalk', 'diorite', 'emery', 'flint', 'gabbro'];
var HEART_RATE_PLATFORMS = ['diorite', 'emery'];

function supported(option, platform) {
  if (option.requires === 'health') return HEALTH_PLATFORMS.indexOf(platform) >= 0;
  if (option.requires === 'heartrate') return HEART_RATE_PLATFORMS.indexOf(platform) >= 0;
  return true;
}

// Copy of the config without the options this watch can't show
function configFor(platform) {
  return JSON.parse(JSON.stringify(clayConfig), function(key, value) {
    if (key === 'options' && Array.isArray(value)) {
      return value.filter(function(o) { return supported(o, platform); })
        .map(function(o) { return { label: o.label, value: o.value }; });
    }
    return value;
  });
}

Pebble.addEventListener('showConfiguration', function() {
  var info = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
  clay.config = configFor(info ? info.platform : 'basalt');
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) { return; }
  Pebble.sendAppMessage(clay.getSettings(e.response), function() {
    console.log('Sent config data to Pebble');
  }, function(error) {
    console.log('Failed to send config data!');
    console.log(JSON.stringify(error));
  });
});

var WEATHER_COMPLICATIONS = ['1', '2', '12', '13', '14', '15', '16', '17'];
var DEFAULT_SLOTS = ['2', '1', '3', '4'];

// True when a panel spot shows weather (checked before fetching it)
function weatherNeeded() {
  var settings = {};
  try {
    settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (err) {}
  for (var i = 0; i < 4; i++) {
    var value = settings['Slot' + (i + 1)];
    if (value === undefined) value = DEFAULT_SLOTS[i];
    if (WEATHER_COMPLICATIONS.indexOf(String(value)) >= 0) return true;
  }
  return false;
}


// The watch asks for weather on its schedule (and when units change).
// Anything fetched in the last 10 minutes is reused, which absorbs bursts.
Pebble.addEventListener('appmessage',
  function(e) {
    console.log("AppMessage received!");
    getWeather(10 * 60 * 1000);
  }
);

var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function weatherCodeToCondition(code, is_day) {
  if (code === 0 && is_day === 1) return 0; // Clear
  if (code === 0 && is_day !== 1) return 1; // Clear Night
  if (code <= 2 && is_day === 1) return 2; // Partly Cloudy
  if (code <= 2 && is_day !== 1) return 3; // Partly Cloudy Night
  if (code <= 3) return 4; // Cloudy
  if (code <= 48) return 5; // Fog
  if (code <= 57) return 6; // Drizzle
  if (code <= 67) return 7; // Rain
  if (code <= 75) return 8; // Snow
  if (code <= 77) return 8; // Snow Grains
  if (code <= 82) return 8; // Showers
  if (code <= 86) return 8; // Snow Shwrs
  if (code === 95) return 9; // T-Storm
  if (code <= 99) return 9; // T-Storm
  return 0;
}

var WEATHER_CACHE_KEY = 'weather';

// The watch reads -1000 as "no value"
var NO_VALUE = -1000;

function orNoValue(v) {
  return typeof v === 'number' && !isNaN(v) ? Math.round(v) : NO_VALUE;
}

// "2026-11-07T06:52" (local time of the location) -> minutes after midnight
function minutesOfDay(iso) {
  var m = typeof iso === 'string' && iso.match(/T(\d\d):(\d\d)/);
  return m ? parseInt(m[1], 10) * 60 + parseInt(m[2], 10) : NO_VALUE;
}

function sendWeather(weather) {
  Pebble.sendAppMessage({
      "Temperature": weather.temperature,
      "Conditions": weather.conditions,
      "TempHigh": weather.high,
      "TempLow": weather.low,
      "Humidity": weather.humidity,
      "RainChance": weather.rain,
      "Sunrise": weather.sunrise,
      "Sunset": weather.sunset
    },
    function(e) {
      console.log("Weather info sent to Pebble successfully!");
    },
    function(e) {
      console.log("Error sending weather info to Pebble!");
    }
  );
}

function cachedWeather() {
  try {
    return JSON.parse(localStorage.getItem(WEATHER_CACHE_KEY));
  } catch (err) {
    return null;
  }
}

function locationSuccess(pos) {
  // Construct URL

  var url = 'https://api.open-meteo.com/v1/forecast?' +
      'latitude=' + pos.coords.latitude +
      '&longitude=' + pos.coords.longitude +
      '&current=temperature_2m,weather_code,is_day,relative_humidity_2m' +
      '&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset' +
      '&timezone=auto&forecast_days=1';
  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      var daily = json.daily || {};
      var first = function(a) { return a ? a[0] : null; };
      var weather = {
        temperature: Math.round(json.current.temperature_2m),
        conditions: weatherCodeToCondition(json.current.weather_code, json.current.is_day),
        high: orNoValue(first(daily.temperature_2m_max)),
        low: orNoValue(first(daily.temperature_2m_min)),
        humidity: orNoValue(json.current.relative_humidity_2m),
        rain: orNoValue(first(daily.precipitation_probability_max)),
        sunrise: minutesOfDay(first(daily.sunrise)),
        sunset: minutesOfDay(first(daily.sunset)),
        time: Date.now()
      };
      try {
        localStorage.setItem(WEATHER_CACHE_KEY, JSON.stringify(weather));
      } catch (err) {}
      sendWeather(weather);
    }      
  );
}

function locationError(err) {
  console.log("Error requesting location!");
}

// Send cached weather if it is younger than maxAgeMs, otherwise fetch it.
function getWeather(maxAgeMs) {
  var cached = cachedWeather();
  // caches from older versions lack the daily fields: fetch again
  if (cached && cached.sunset !== undefined && Date.now() - cached.time < maxAgeMs) {
    console.log("Using cached weather");
    sendWeather(cached);
    return;
  }
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    // Weather doesn't need a fresh fix: reuse a position up to 30 minutes old
    // and skip high-accuracy GPS to save phone battery.
    {timeout: 15000, maximumAge: 30 * 60 * 1000, enableHighAccuracy: false}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // The watchface restarts after every notification or app, so reuse
    // weather fetched in the last 30 minutes instead of fetching again.
    if (weatherNeeded()) {
      getWeather(30 * 60 * 1000);
    }
  }
);