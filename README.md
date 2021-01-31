# Mara X Meter

The [Lelit Mara X](https://marax.lelit.com/) is a heat exchanger (hx) espresso machine. It has received a lot of attention, as it has two different modes of operation and due to that, requires more sensors than an usual hx espresso machine. Several resources on the internet showed, that one can extract data of the sensors via a serial connection.

With this project you can visualize the read data on an e-ink display and even automatically start a shot timer.

## Motivation

Pulling a perfect espresso (often referred to as god shot) requires a lot of patience and trial and error. Several parameter come into play for a perfect cup of coffee. Two of those factors with an espresso machine are the brewing temperature and the extraction time (or shot time).

Since the machine enables access to the current heat exchanger temperature(among others), I wanted to visualize it on a display and try to find a way to extract the pump running time, which indicates the shot time.

Some inspiration for this project:

- [Lelit Insider](https://www.youtube.com/watch?v=9NL6yeq7sMM): This is probably the origin of most mara x modifications including the data extraction. It shows how the connection to the mara x can be done with a laptop.
- [This reddit post](https://www.reddit.com/r/espresso/comments/hft5zv/data_visualisation_lelit_marax_mod/) shows how a mara x owner extracts the data, stores it to a database and visualizes it.
- [Another github repository](https://github.com/alexrus/marax_timer) shows how the data is visualized on an oled display and also includes, how the shot timer is triggered by extracting the pump running state.

Since I wanted the shot timer and the graphs, I decided to create my own project. To reduce the visual impact on the looks of the espresso machine, I decided to go with an e-ink display.

## Solution

Similar to my [coffee grinder automation](https://github.com/Jerey/coffee-automation), I again chose an D1 mini as basis for this project. It is used to read the data from the espresso machine and visualize it on the display. Because of that, you can connect to it via wifi, and therefore flash it over the air (OTA).

## Setup

To build this project, following parts are needed:

- D1 mini (or a similar ESP device such as the node mcu)
- A reed sensor
- An e ink display (I chose the Waveshare 4.2 module with two colors)
- A mara x

### Wiring

Following you can see a schematic for the wiring. It is setup for a D1 mini, but can also be adapted for any other ESP device.

![Schematic of the mara x meter](/documentation/mara-x-meter.svg)

Here is a picture to know, what to connect where:

<!-- ![Connection from the bottom](/documentation/CloseupMaraXConnector.png) -->
<img src="/documentation/CloseupMaraXConnector.png" width="400">

Here another picture from further away:

<!-- ![Bottom of the mara-x](/documentation/MaraXBottom.png) -->
<img src="/documentation/MaraXBottom.png" width="400">

| In case I somehow messed up the pin numbering for the RX and TX, and you get bad values only, please try to swap the pins.

## Further ideas

### Shot timer from pump switch

### Mqtt, nodered, grafana and influxdb

## Changes to the pins compared to original:

### NodeMCU:

Old

`BUSY -> GPIO4, RST -> GPIO2, DC -> GPIO0, CS -> GPIO15, CLK -> GPIO14, DIN -> GPIO13, GND -> GND, 3.3V -> 3.3V`

New

`BUSY -> GPIO4/D2, RST -> GPIO12/D6, DC -> GPIO0/D3, CS -> GPIO5/D1, CLK -> GPIO14/D5, DIN -> GPIO13/D7, GND -> GND, 3.3V -> 3.3V`

```cpp
GxIO_Class io(SPI, /*CS=D8*/ /*SS*/5, /*DC=D3*/ 0, /*RST=D4*/ 12); // arbitrary selection of D3(=0), D4(=2), selected for default of GxEPD_Class
GxEPD_Class display(io, /*RST=D4*/ 12, /*BUSY=D2*/ 4); // default selection of D4(=2), D2(=4)
```

### D1 Mini V3

Old

`BUSY -> D2, RST -> D4, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V`

New

`BUSY -> D2, RST -> D1, DC -> D3, CS -> D8, CLK -> D5, DIN -> D7, GND -> GND, 3.3V -> 3.3V`

```cpp
GxIO_Class io(SPI, /*CS=D8*/ SS, /*DC=D3*/ 0, /*RST=D1*/ 5); // arbitrary selection of D3(=0), D4(=2), selected for default of GxEPD_Class
GxEPD_Class display(io, /*RST=D1*/ 5, /*BUSY=D2*/ 4); // default selection of D4(=2), D2(=4)
```

#### Schematic

![Schematic of the mara x meter](/documentation/mara-x-meter.svg)

//<img src="/documentation/mara-x-meter.svg" width="400">

// BUSY -> D2,
// Busy Lila
Passt
// RST -> D1,
// Rst Weiß
Passt
// DC -> D3,
// DC Grün
Passt
// CS -> D8,
// CS Orange
Passt
// CLK -> D5,
// CLK Geld
Passt
// DIN -> D7,
// DIN Blau
Passt
// GND -> GND,
// GND Braun
Passt
// 3.3V -> 3.3V
// VCC Grau
Passt
