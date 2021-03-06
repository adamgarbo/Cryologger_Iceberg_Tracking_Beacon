# Cryologger - Iceberg Tracking Beacon (ITB) v1.0
![Image](https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/2018_300434063418130_A.JPG)

## Background
Icebergs and ice islands represent significant hazards to marine navigation and offshore infrastructure at a time when demand for access to Canada’s Arctic waters is increasing.

There is a growing demand for in situ iceberg tracking data to monitor their drift trajectory and improve predictions of ice hazard occurrence and behaviour, yet the high cost of commercial tracking devices often prevents monitoring at optimal spatial and temporal resolutions.

## Objective
The goal of this project is to determine if the Cryologger, a tracking beacon based on inexpensive, open-source hardware and software, can provide a reliable and cost-effective platform for monitoring the drift of icebergs and ice islands in the Canadian Arctic.

## Design
The Cryologger is based on the open-source Arduino platform (www.arduino.cc) and built using low-cost, do-it-yourself electronics that can be easily modified to meet the needs of the end-user. Code was written using the Arduino Integrated Development Environment (IDE) and benefits from the availability of community-generated libraries.

Planned for extended deployments in harsh Arctic conditions, the Cryologger can provide long-term measurements of GPS position, temperature, pressure, pitch, roll, tilt-compensated heading and battery voltage. 

### Bill of Materials

**Table 1.** Bill of materials
| Component | Product | Cost (USD) | URL |
| --- | --- | :---: | --- |
| Satellite transceiver | Rock7 RockBLOCK 9603 | $249.95 | http://www.rock7mobile.com/products-rockblock-9603 |
| Satellite antenna | Maxtena	M1621HCT-P-SMA | $50.00 | https://www.maxtena.com/products/f-passive/m1621hct-p-sma-iridium-passive-antenna/ |
| Processor | Adafruit	Pro Trinket - 3V 12MHz | $9.95 | https://www.adafruit.com/products/2010 |
| GNSS | Adafruit	Ultimate GPS Breakout | $39.95 | https://www.adafruit.com/products/746 |
| IMU | Adafruit LSM303 - Triple-axis Accelerometer + Magnetometer | $14.95 | https://www.adafruit.com/products/1120 |
| Real-time clock IC |	Maxim	DS3231SN |	$9.00 |	https://www.digikey.ca/product-detail/en/maxim-integrated/DS3231SN-T-R/DS3231SN-T-RCT-ND/3894827
| Real-time clock PCB	| Adafruit SMT Breakout PCB for SOIC-16 | $3.95	|	https://www.adafruit.com/product/1207
| Real-time clock battery	| Adafruit	20mm Coin Cell Breakout Board (CR2032)	|	$2.50	| https://www.adafruit.com/product/1870
| EEPROM | Microchip	256K I2C EEPROM 24LC256 | $1.25 | https://www.digikey.ca/en/products/detail/microchip-technology/24LC256-I-P/273431 |
| Sensors | Adafruit MPL3115A2 Pressure/Altitude/Temperature Sensor | $9.95 | https://www.adafruit.com/product/1893 |
| Voltage Regulator | 5V, 2.5A Step-Down Voltage Regulator D24V22F5 | $8.95 | https://www.pololu.com/product/2858 |
| Power connector | Male DC Power adapter - 2.1mm plug to screw terminal block | $2.00 | https://www.adafruit.com/product/369 |
| Power connector | Breadboard-friendly 2.1mm DC barrel jack | $0.95 | https://www.adafruit.com/product/373 |
| Protoboard | Adafruit	Perma-Proto Full-sized Breadboard PCB | $5.95	| https://www.adafruit.com/product/1606 |
| Battery | Tadiran Lithium Pulses Plus 7.2V 38Ah 273.6Wh | $100.00 | http://www.tadiranbat.com/assets/tlp-93121-b-al1.pdf |
| Enclosure | Nanuk 905 | $45.95 | https://nanuk.com/collections/all-nanuk-cases/products/nanuk-905 |
| **Total** | | **$555.25** | 

### Assembly

<p align="center"><img src="https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/assembly_1.JPG" width="480"></p>
<p align="center"><b>Figure 1.</b> Finished assembly of Cryologger components.</p>
<p align="center"><img src="https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/assembly_2.JPG" width="480"></p>
<p align="center"><b>Figure 2.</b> Cryologger beacons situated inside a 3D-printed case and Nanuk enclosures prior to deployment.</p>

### Measurements

**Table 2.** Collected measurements and the amount of memory required to transmit.
| Variable | Unit | Comments | Size |
| --- | :---: | --- | --- |
| Datetime  |   | UNIX Epoch Time | 4 bytes |
| Temperature | °C  | Internal temperature | 2 bytes |
| Pressure | hPa | Internal pressure | 2 bytes |
| Pitch | °| Pitch | 2 bytes |
| Roll | ° | Roll | 2 bytes |
| Heading | °  | Tilt-compensated heading (0-360°) | 2 bytes |
| Latitude | DD | GPS latitude | 4 bytes |
| Longitude | DD | GPS longitude | 4 bytes |
| Satellites | # | Number of satellites in view  | 2 bytes |
| HDOP |  | Horizonal dilution of precision | 2 bytes |
| Voltage | V | Battery voltage | 2 bytes |
| Transmit duration  | s | Transmission time of previous Iridium SBD message  |  2 bytes |
| Message counter |  | Number of transmitted messages |  2 bytes |
| | | | **Total: 30 bytes** |

### Data Transmission and Processing
Sensor measurements and GPS position are recorded hourly and stored in memory until the desired transmission interval is reached. Data are compressed into a binary message (max 340 bytes) to minimize the cost and the total number of transmissions required. Data are transmitted via the Iridium Short Burst Data (SBD) satellite network at user-specified intervals, which can be remotely updated based on the desired sampling frequency. Once the SBD data messages are received by an Iridium ground station, they are sent to Rock7's server. The data is then forwarded to an Amazon Web Services (AWS) SQS queue, decoded using an AWS Lambda Python function, and stored in a database using the Amazon Relational Database Service (RDS). 

Data is made freely available and can be viewed in near-real time at https://cryologger.org.

## Deployments

A total of 6 Cryologger drift tracking beacons were deployed from the CCGS Amundsen’s helicopter on icebergs and ice islands along the coasts of Ellesmere Island and Baffin Island during the ArcticNet leg of the 2018 Amundsen Expedition. 

<p align="center"><img src="https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/2018_deployments.png" width="480"></p>
<p align="center"><b>Figure 3.</b> 2018 Cryologger deployment locations.<p>
  
The overall suitability of potential targets was determined by assessing the iceberg’s size, shape and location. At the time of deployment, a compass heading of the tracking beacon was recorded and a 360° aerial photo survey of the iceberg was performed if possible. Following deployment, communications with each beacon were successfully established in order to remotely modify their operational parameters for optimal battery efficiency.

<p align="center">
  <img src="https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/2018_300434063415110.JPG" width="420">
  <img src="https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/2018_300434063411050.JPG" width="420">
</p>
<p align="center"><b>Figure 4.</b> Suitable iceberg targets to be instrumented with Cryologger beacons.</p>

## Results

As of December 2020, 5 of 6 Cryologger beacons have been lost due to iceberg break-up or deterioration. The single remaining operational beacon was disloged from its iceberg and is currently transmitting its location from shore. Combined, the six Cryologgers have transmitted more than 54,000 GPS positions via the Iridium satellite network and have achieved a lifespan ranging from 310 to 841 days. 

<p align="center"><img src="https://github.com/adamgarbo/Cryologger_Iceberg_Tracking_Beacon/blob/main/Archive/v1.0/Images/2018_trajectories.png" width="480"></p>
<p align="center"><b>Figure 5.</b> Cryologger trajectories between September 2018 and December 2020.</p>

**Table 3.** Cryologger v1.0 deployments and days operational as of December 20, 2020.
| Beacon	| Deployment	| Latitude	| Longitude	| Days Operational | Comments
| :---: | :---: | :---: | :---: | :---: | --- | 
| 18130	| 2018-09-03	| 66.2188	| -61.4864	| 435 | 
| 15110	| 2018-08-27	| 75.7712	| -78.5163	| 310 | Iceberg deteriorated off the coast of Labrador after travelling over 4000 km
| 19120	| 2018-08-28	| 77.6074	| -76.9739	| 709 | Ice island catastrophically broke apart after being caught in a gyre in Baffin Bay
| 11050	| 2018-08-28	| 77.9703	| -78.4877	| 748 | Iceberg broke in half between September 10-15, 2020
| 15160	| 2018-09-01	| 67.3577	| -63.271	| 841 | Beacon was disloged from the iceberg and is currently transmitting from shore
| 16060	| 2018-09-01	| 67.5387	| -63.3641	| 386 | 

## Conclusion & Future Work
The success of the Cryologger tracking beacon deployments have demonstrated that low-cost, open-source hardware and software can provide a robust and cost-effective platform for the collection in-situ iceberg tracking data. These data can provide key insights to iceberg drift and deterioration processes in the Canadian Arctic.

Development of the next version of the Cryologger is currently underway and will incorporate the Adafruit Feather ecosystem of development boards to improve overall processing capabilities and modularity.

## Repository Contents

* **/Software** - Contains the Arduino code

* **/Bill of Materials** - Information on all components used construction and their associated costs

* **/Documentation** - All project-related documents

* **/Hardware** - Autodesk EAGLE schematics and Fusion 360 design files

## Documentation
* Currently in progress

## License Information
This project is distributed under the GNU General Public License v3.0.

Cheers,

**Adam**
