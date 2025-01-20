# ProjektR
Sve potrebno za izvrsavanje ProjektR-a

Vizualizacije sinkronizacijskih problema koristeći FreeRTOS, putem ESP32 NodeMCU, Espressif - IDF

Navedeni problemi Sleeping barber problem i Dining philosophers

Za pokrenuti program clonati repozitorij, zatim pokrenuti idf build, idf flash.

Pritiskom na Lijevi gumb (po diagramu) će se vrtiti Menu s navedenim programima.
Pritiskom na Desni gumb (po diagramu) će se pokrenuti izabrani program.
Ponovnim pritiskom na Desni gumb programi se prekidaju i vraća se u glavni izbornik.


Sleeping Barber problem je riješen uz pomoću FreeRTOS Qeueue.

Dining Philosopher problem je riješen uz pomoć FreeRTOS Mutex-a i Semaphore-a.

Ispisi koriste FreeRTOS task notifications-e.

Gumbi i prekidi su ostvareni preko Interrupt Service Routine-a.


CircuitDiagram.jpg
![Alt text](CircuitDiagram.jpg?raw=true "Circuit Diagram")
