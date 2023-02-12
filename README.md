# display-ding

I created this to use a Heltec ESP8266 board with an OLED display to show the Status of my laundry machine.
[I used this board from az-delivery](https://www.az-delivery.de/products/esp8266-mikrocontroller-mit-integrierten-0-91-oled-display).
Text on the display is in German, but feel free to fork this repo and change to your likings.

![display-ding-scheduled](https://user-images.githubusercontent.com/54552094/218286072-19b7fbf4-4b58-4602-a8a9-38126276f77a.png)

![display-ding2](https://user-images.githubusercontent.com/54552094/218286075-4c2f86c4-8322-49fd-ba71-a689c86f88eb.gif)

## Features

- Shows cute symbol of a laundry machine
- Laundry machine symbol is animated, if the laundry machine is running
- Another animation when the laundry machine is finished
- Screen shuts off after 1 hour if the laundry machine is off
- Uses MQTT to determine the status of the laundry machine
- Uses MQTT with username/password

## Setup

- Rename `config.h.TEMPLATE` to `config.h` and put your WiFi and MQTT settings in.
- Build and flash on your board.

## MQTT

Your MQTT Publisher should publish a JSON like this:

```json
"status": 0,
"timestamp": "2023[...]"
```

The timestamp is actually not used. I might use it later.
So you could remove this line from the code: 
https://github.com/diecknet/display-ding/blob/57161b71aff0afef8537ad7e3a7dcd5a7139fb86/src/main.cpp#L112

I publish my laundry machine status using MQTT.Publish on Home Assistant:

```yaml
service: mqtt.publish
data_template:
  payload: >-
    { "status": {{ states('input_number.waschmaschinenstatus') | int }},
    "timestamp": "{{ states.input_number.waschmaschinenstatus.last_changed }}" }
  topic: waschmaschine/status
  retain: true
enabled: true
```
