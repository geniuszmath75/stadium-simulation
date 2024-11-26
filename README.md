
# Temat 11 - Stadion

## 1. Opis projektu: 

Na stadionie piłkarskim rozegrany ma zostać mecz finałowy Ligi Mistrzów. Z uwagi na rangę imprezy ustalono następujące rygorystyczne zasady bezpieczeństwa.
* Na stadionie może przebywać maksymalnie K kibiców.
* Wejście na stadion możliwe będzie tylko po przejściu drobiazgowej kontroli, mającej zapobiec wnoszeniu przedmiotów niebezpiecznych.
* Kontrola przy wejściu jest przeprowadzana równolegle na 3 stanowiskach, na każdym z nich mogą znajdować się równocześnie maksymalnie 3 osoby.
* Jeśli kontrolowana jest więcej niż 1 osoba równocześnie na stanowisku, to należy zagwarantować by byli to kibice tej samej drużyny.
* Kibic oczekujący na kontrolę może przepuścić w kolejce maksymalnie 5 innych kibiców. Dłuższe czekanie wywołuje jego frustrację i agresywne zachowanie, którego należy unikać za wszelką cenę.
* Istnieje niewielka liczba kibiców VIP (mniejsza niż 0,5% * K), którzy wchodzą (i wychodzą) na stadion osobnym wejściem i nie przechodzą kontroli bezpieczeństwa.
* Dzieci poniżej 15 roku życia wchodzą na stadion pod opieką osoby dorosłej.

Po wydaniu przez kierownika polecenia (sygnał1) pracownik techniczny wstrzymuje wpuszczanie kibiców. Po wydaniu przez kierownika polecenia (sygnał2) pracownik techniczny wznawia wpuszczanie kibiców. Po wydaniu przez kierownika polecenia (sygnał3) wszyscy kibice opuszczają stadion – w momencie gdy wszyscy kibice opuścili stadion, pracownik techniczny wysyła informację do kierownika.
Napisz odpowiedni program kibica, pracownika technicznego i kierownika stadionu.. Wszelkie mechanizmy synchronizacyjne są inicjowane na początku przez pracownika technicznego (osobny program).

## 2. Opis testów:

* **Test_1** - sprawdzenie, czy program odrzuca nadmiar kibiców (Liczba kibiców <= K).
* **Test_2** - symulacja sytuacji, gdy stanowiska przestają działać (test semaforów).
* **Test_3** - sprawdzenie poprawności odbioru poleceń od kierownika przez pracownika technicznego (obsługa sygnałów).
* **Test_4** - sprawdzenie, czy osoby VIP są wpuszczane bez kolejki, a dzieci z opiekunami.
* **Test_5** - sprawdzenie, czy kibic czekający w kolejce przepuści max. 5 osób
