# Malfunction Checker
The program detects and signals hydraulic malfunctions in a heating system by keeping track of time passed since last activation. 

An inner timer counts powers a pin if the timer elapses. The timer's start value is calculated as milliseconds = (-slope * potentiometer_value + mustajarvi_variable) * 3600, where potentiometer_value is read from a connected potentiometer and slope/mustajarvi_variable are adjustable using the Arduino board and saved in EEPROM memory. The variables are used to fine-tune how the potentiometer value corresponds to timer length.

The program has a menu button that cycles through timer display, slope variable input and mustajarvi_variable input. There are also buttons for manual timer reset, displaying current potentiometer value, manual relay signal activation (fault signal) and clearing EEPROM memory. The last two buttons also include a confirmation stage to safeguard risky operations.

The program was made with usability in mind and it's operation should be informative and intuitive enough for use without a manual.