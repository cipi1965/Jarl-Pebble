// Import the Clay package
var Clay = require('@rebble/clay');
// Load our Clay configuration file
var clayConfig = require('./config');
// Initialize Clay
var clay = new Clay(clayConfig);


// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log("AppMessage received!");
		getWeather();
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

function locationSuccess(pos) {
  // Construct URL

  var url = 'https://api.open-meteo.com/v1/forecast?' +
      'latitude=' + pos.coords.latitude +
      '&longitude=' + pos.coords.longitude +
      '&current=temperature_2m,weather_code,is_day';
  // Send request to OpenWeatherMap
  xhrRequest(url, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      var conditions = weatherCodeToCondition(json.current.weather_code, json.current.is_day);
			
      // Assemble dictionary using our keys
      var dictionary = {
          "Temperature": json.current.temperature_2m,
          "Conditions": conditions
      };

      // Send to Pebble
      Pebble.sendAppMessage(dictionary,
        function(e) {
            console.log("Weather info sent to Pebble successfully!");
        },
        function(e) {
            console.log("Error sending weather info to Pebble!");
        }
      );
    }      
  );
}

function locationError(err) {
  console.log("Error requesting location!");
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log("PebbleKit JS ready!");

    // Get the initial weather
    getWeather();
  }
);