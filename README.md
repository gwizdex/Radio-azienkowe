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
