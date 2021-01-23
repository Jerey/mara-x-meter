# EPaperPlayground

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