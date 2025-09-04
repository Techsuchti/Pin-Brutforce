# Pin-Brutforce


ESP32-C3 – Web-konfigurierbare Test-Firmware (UART/LED, Timing & Debug)

Der Sketch ist eine ESP32-C3-Firmware mit Web-Oberfläche, die einen vierstelligen PIN-Bereich (z. B. 0000–9999) automatisiert durchprobiert. Jede PIN wird dabei als Bitmuster über einen LED-Ausgang (typisch: LED/IR-LED an einer optischen Schnittstelle) „geblinkt“. Parallel lauscht der ESP32 auf einer UART-Schnittstelle und schaut, ob vom angeschlossenen Gerät (z. B. ein Stromzähler/Testgerät) eine reaktionsartige Rückmeldung kommt. Wenn ja, wird die aktuelle PIN als „gefunden“ notiert und der Lauf gestoppt.

Was genau macht der Code?

WLAN & Web-UI: Beim ersten Start öffnet er ein Wi-Fi-Portal (SSID „StromBruteforce“), danach verbindet er sich automatisch. Über ein schlichtes Web-Interface (Dashboard, Einstellungen) kannst du Pins, Baudrate, Timing und Debug-Level setzen sowie den Lauf Start/Stop bedienen.

Speicher (SPIFFS): Einstellungen liegen in /config.json, gefundene Kandidaten in /found.txt und werden beim Booten geladen/weitergeführt.
„Zählertyp“-Profile: Eine interne Liste mit Gerätenamen/Protokollinfos dient primär dazu, passende Baudraten/Defaults zu wählen (Komfortfunktion).
PIN-Durchlauf: Für jede Zahl wird über blinkCode() ein 4-Bit-Muster pro Ziffer auf dem LED-Pin ausgegeben (konfigurierbare Impuls-/Pausenzeiten).
Antwortprüfung: Der UART-Eingang wird mitgelesen; kommt eine „hinreichend lange“ Zeile an, zählt das als Treffer, die PIN wird gespeichert und der Vorgang beendet.
APIs: /api/status liefert JSON-Status (running/tested/current/found), /api/term gibt den Roh-„Terminal“-Puffer aus.
Wozu ist das gedacht?
Technische Tests/Forschung am eigenen, autorisierten Prüfaufbau: z. B. um Timing, Signalwege und Antwortverhalten einer optischen/seriellen Schnittstelle zu untersuchen, die Robustheit von PIN-Eingaben zu evaluieren oder eine Labor-Demu für Web-gesteuerte Testläufe zu haben.
Nicht für den Einsatz an fremden oder produktiven Messgeräten bestimmt. Das Umgehen von Schutzmechanismen an Versorgungsanlagen kann rechtswidrig sein.
