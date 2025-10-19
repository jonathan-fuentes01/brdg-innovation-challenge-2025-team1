# BRDG Innovation Challenge - Team 1 - $1750 winner

Our topics for the BRDG Innovation Challenge 2025 was on urban green integration, specifically how to sustain greenery in urban landscapes. Our product was Xylobench, a bench model that is able to maintain green life through an automated irrigation system.

## How does this system work?

The system works by having a 24v battery that is able to supply voltage to both the ESP32 microcontroller and the pump itself through a buck-converter module to step down the voltage and a relay that is controlled by the microcontroller to control the pump activation. To measure the dryness of the soil, there is a capacitive soil moisture sensor that senses the moisture within the soil. The ESP32 supports internal ADC, and the sensor is connected to the corresponding pin. To filter peaks in noise within the sensor readings, we took 7 readings from the analog signal, stored them in an array, sorted the array, and then took the median value within that array to minimize noisy values.
