# Mara X Meter

The [Lelit Mara X](https://marax.lelit.com/) is a heat exchanger (hx) espresso machine. It has received a lot of attention, as it has two different modes of operation and due to that, requires more sensors than an usual hx espresso machine. Several resources on the internet showed, that one can extract data of the sensors via a serial connection.

With this project you can visualize the read data on an e-ink display and even automatically start a shot timer.

## Motivation

Pulling a perfect espresso (often referred to as a god shot) requires a lot of patience and trial and error. Several parameter come into play for a perfect cup of coffee. Two of those factors with an espresso machine are the brewing temperature and the extraction time (or shot time).

Since the machine enables access to the current heat exchanger temperature(among others), I wanted to visualize it on a display and try to find a way to extract the pump running time, which indicates the shot time.

Some inspiration for this project:

- [Lelit Insider](https://www.youtube.com/watch?v=9NL6yeq7sMM): This is probably the origin of most mara-x modifications including the data extraction. It shows how the connection to the mara-x can be done with a laptop.
- [This reddit post](https://www.reddit.com/r/espresso/comments/hft5zv/data_visualisation_lelit_marax_mod/) shows how a mara-x owner extracts the data, stores it to a database and visualizes it.
- [Another github repository](https://github.com/alexrus/marax_timer) shows how the data is visualized on an oled display and also includes, how the shot timer is triggered by extracting the pump running state.

Since I wanted the shot timer and the graphs, I decided to create my own project. To reduce the visual impact on the looks of the espresso machine, I decided to go with an e-ink display.

## Solution

Similar to my [coffee grinder automation](https://github.com/Jerey/coffee-automation), I again chose an D1 mini as basis for this project. It is used to read the data from the espresso machine and visualize it on the display. Because of that, you can connect to it via wifi, and therefore flash it over the air (OTA).

Here you can see a sample video taken while the mara-x was heating up.

![](/documentation/SampleHeatup.gif)

> This video was taken over the course of 30 minutes. Further the machine was already a bit heated up from a previous shot.

## Workflow

My mara x is connected to a wifi controlled power outlet. Whenever I want a coffee, I switch it on and after I am done making my coffee, I switch the outlet off. Since e-ink displays have to be shutdown properly to avoid pixel burn, the D1 mini has to survive around 5s after loosing power to do that. Since batteries usually have a to big capacity for that and are also limited in number of charging cycles, I looked into supercapacitors after [Oliver Ju](https://github.com/helmo2004) suggested those.

## Setup

To build this project, following parts are needed:

- D1 mini (or a similar ESP device such as the node mcu)
- A reed sensor
- An e ink display (I chose the Waveshare 4.2 module with two colors)
- A mara-x
- A supercapacitor (~ 5F or bigger)
- A resistor (~ 5Ω)

> I used two supercapacitors with 2.7V and 10F in Series - as this results in a higher voltage capability (up to 5.4V) - but therefore also a smaller capacity (5F).

### Wiring

Following you can see a schematic for the wiring. It is setup for a D1 mini, but can also be adapted for any other ESP device.

![Schematic of the mara-x meter](/documentation/mara-x-meter.svg)

Here is a picture to know, what to connect where at the mara-x:

<img src="/documentation/CloseupMaraXConnector.png" width="700">

Here another picture further away:

<img src="/documentation/MaraXBottom.png" width="700">

#### Reed sensor

The reed sensor is glued onto the vibration pump. Ensure, that you can read `0`'s and `1`'s while activating the pump from the reed sensor. I had to look for a good position to receive any values.

#### Supercapacitor

The capacitor between 3.3V and ground is needed to properly switch off the e-ink display to avoid pixel burn. Whenever the D1 mini recognizes a power loss, it will automatically shutdown the display. The resistor is needed to limit the current consumed by the capacitor (which have a very low [ESR](https://en.wikipedia.org/wiki/Equivalent_series_resistance) of 60mΩ each - so in total 120mΩ), as the d1 mini otherwise will not switch on. Depending on your setup, this value may vary.

> For me, I had cables in my prototype with a rather high resistance and also the chosen supercapacitor has an internal resistance. The bigger the resistance, the quicker the capacitor discharges through which the d1 mini might not have enough time to switch off the display.

### Flashing

Initially you will have to flash the software to the D1 mini via usb. Then on the next start, the D1 mini will open up an access point. The password and the ssid name can be found in the code. After connecting to the D1 mini, a wifi configuration page will pop up and you can connect the D1 mini to your wifi. From then on, the D1 mini will automatically connect to your wifi, if available, and you can flash over the air.

The project is built with [platform io](https://docs.platformio.org/en/latest/core/index.html). You can find installation information there.

> It can happen, that you have to specify the `upload_port` in the `platformio.ini`.

## Further ideas

This project has several parts, which can be extended. Here are some ideas, I might extend one day, but for now I am happy with the current state and will build a case for the display and d1 mini.

### Shot timer from pump switch

The reed sensor alters between 1's and 0's while the pump is running. Therefore one has to wait for a certain time, until the pump off is certain. Further, the pump sometimes is activated by the machine itself to exchange the water in the HX.

To get a more precise shot timer, one could check, whether it is possible to listen to the shot lever directly.

### Mqtt, NodeRed, Grafana and InfluxDB

Since I already have a Mqtt, NodeRed, Grafana and an InfluxDB up and running, it would be rather straight forward to publish the data via the network and visualize it in Grafana. This could be useful in a few cases:

- By publishing the data, one could configure a notification, when a certain HX temperature has been reached and been stable for a given time.
- One could automatically create a table to rate the shots. The given HX temperature with the shot timer could be used as reference.
