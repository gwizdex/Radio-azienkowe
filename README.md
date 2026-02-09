# Radio-azienkowe

Jesto to projekt radia do łązienki.

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

<img width="827" height="1235" alt="obraz" src="https://github.com/user-attachments/assets/cd736d84-6096-4187-96a0-e043e82894fe" />
<img width="829" height="1034" alt="obraz" src="https://github.com/user-attachments/assets/2e6fe498-be6a-48a3-bd96-93f422fcb6f6" />
<img width="837" height="666" alt="obraz" src="https://github.com/user-attachments/assets/84d8f45a-ab84-45e8-8cae-4e6bc50cc878" />
<img width="855" height="682" alt="obraz" src="https://github.com/user-attachments/assets/6e302d6d-9e50-4858-b4db-fa398aeaf7cf" />
<img width="828" height="987" alt="obraz" src="https://github.com/user-attachments/assets/6875e549-ed59-422d-b6a6-1d9eab9ef306" />
<img width="838" height="832" alt="obraz" src="https://github.com/user-attachments/assets/92744017-2229-4d89-9c5d-f3fff51ccf1d" />
<img width="827" height="1311" alt="obraz" src="https://github.com/user-attachments/assets/8b3cff2f-27d4-421e-8d70-e7de1060ec3a" />


