# Radio-łazienkowe

Jesto to projekt radia do łązienki. <img width="101" height="69" alt="obraz" src="https://github.com/user-attachments/assets/56c87561-6b80-47ea-a032-c84eab18e1a0" />

Oparty jest o płytkę ESP32-S3 WROOM-1 N16R8 oraz MAX98357A i czujnik światła BH1750. Głośnik jaki został użyty to 4Ω głośnik do pracy w saunach.

Zasada działania: Czujnik oświetlenia wykrywa wartość powyżej ustawinego progu i rozpoczyna odtwarzania strumienia audio ze wcześnie zdefiniowanego adresu.
Mamy wstępnie wpisane cztery przykładowe stacje radiowe, można je dowolnie modyfikować.
Oprogramowanie daje możliwość ustawienie trybu pracy auto/manual. Wybór języka interfejsu, regulację głośności. 
Wszystkie wartości są zapamiętywane i obowiązują po restarcie.

Mamy również do dyspozycji tryb serwisowy, który daje nam możliwość edycji GPIO według własnych upodobań, dodanie czujnika DHT22 i możliwość współpracy z Mqtt.

Mamy również możliwość zmiany sieci WiFi. Do trybu serwisowego trzeba wpisać hasło: "jolka"
Przy pierwszym uruchomieniu uruchamia się tryb AP, szukany sieci o nazwie: "Radio_Config" i hasło: "password123"
Po uzyskaniu połączenia otrzymamy komunikat głosowy o uzyskanym dresie IP w dwóch językach polskim i angielskim.
Stacje radiowe można znależć tu: https://fmstream.org/

This is a bathroom radio project.<img width="112" height="77" alt="obraz" src="https://github.com/user-attachments/assets/85d266f2-89bc-47e9-a971-574fa8c852a2" />


It's based on the ESP32-S3 WROOM-1 N16R8 board, the MAX98357A, and the BH1750 light sensor. The speaker used is a 4Ω speaker suitable for saunas.

Operation: The light sensor detects a value above a set threshold and starts playing an audio stream from a pre-defined address.
Four sample radio stations are pre-programmed, and these can be freely modified.
The software allows you to set auto/manual operation modes, select the interface language, and adjust the volume.
All values ​​are saved and remain valid after a reboot.

We also have a service mode, which allows you to edit the GPIOs to your liking, add a DHT22 sensor, and enable MQTT compatibility.

We can also change the WiFi network. To enter service mode, enter the password: "jolka."
When you first turn it on, it launches AP mode, searches for a network named "Radio_Config" and enters the password: "password123."
Once connected, you'll receive a voice message announcing the IP address you've acquired, in both Polish and English.
Radio stations can be found here: https://fmstream.org/


<img width="364" height="768" alt="obraz" src="https://github.com/user-attachments/assets/1b45c4a7-7a9e-4599-965d-3c1414bb8b17" />
<img width="311" height="330" alt="obraz" src="https://github.com/user-attachments/assets/81fc124d-c8ea-4546-96f1-82c82b3038e2" />
<img width="339" height="482" alt="obraz" src="https://github.com/user-attachments/assets/202bf12d-0967-467e-8f24-0e9e915ae188" />
<img width="682" height="560" alt="obraz" src="https://github.com/user-attachments/assets/150a7881-c7fb-4bcb-83f1-071366da3fd1" />

<img width="827" height="1235" alt="obraz" src="https://github.com/user-attachments/assets/cd736d84-6096-4187-96a0-e043e82894fe" />
<img width="829" height="1034" alt="obraz" src="https://github.com/user-attachments/assets/2e6fe498-be6a-48a3-bd96-93f422fcb6f6" />
<img width="837" height="666" alt="obraz" src="https://github.com/user-attachments/assets/84d8f45a-ab84-45e8-8cae-4e6bc50cc878" />
<img width="855" height="682" alt="obraz" src="https://github.com/user-attachments/assets/6e302d6d-9e50-4858-b4db-fa398aeaf7cf" />
<img width="828" height="987" alt="obraz" src="https://github.com/user-attachments/assets/6875e549-ed59-422d-b6a6-1d9eab9ef306" />
<img width="838" height="832" alt="obraz" src="https://github.com/user-attachments/assets/92744017-2229-4d89-9c5d-f3fff51ccf1d" />
<img width="827" height="1311" alt="obraz" src="https://github.com/user-attachments/assets/8b3cff2f-27d4-421e-8d70-e7de1060ec3a" />


