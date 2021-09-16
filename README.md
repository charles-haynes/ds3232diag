# ds3232diag

Diagnostic for the DS3232 RTC

Displays the status of the DS3232 on the Serial port. First two lines are the initial status, then tries to reset the RTC to a known state similar to the default power on state except that RS1 and RS2 are set to 00 (1Hz) rather than 11 (8.192kHz) and OSF, BB32kHz, and EN32kHz are all turned off. See the [datasheet](DS3232.pdf) for details.
