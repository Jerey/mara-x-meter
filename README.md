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
