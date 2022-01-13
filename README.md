# ds3232diag

Diagnostic for the DS3232 RTC

Displays the status of the DS3232 on the Serial port. First two lines are the initial status, then tries to reset the RTC to a known state similar to the default power on state except that RS1 and RS2 are set to 00 (1Hz) rather than 11 (8.192kHz) and OSF, BB32kHz, and EN32kHz are all turned off. See the [datasheet](DS3232.pdf) for details.

# quickstart

1. Clone this repo.
2. Use [pipx][pipx] to install dependencies:

       pipx install platformio

3. Plug in your watch.
4. Now cd to the root of this repo and run `pio run`. Then `pio run -t upload`
   to upload the sketch to your watch.
5. Start `pio device monitor` to start monitoring your watchy.


[pipx]: https://pypa.github.io/pipx/installation/
