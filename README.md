# Magneto — Meteora Plugins

Tape echo VST3 (Windows e macOS) e AU (macOS), con un suono a metà tra Echoplex EP-3 e Roland RE-201.
La grafica è quella del pannello Magneto. Tutti i comandi della foto sono funzionanti.

## Come compilarlo

1. Carica il contenuto di questa cartella in un nuovo repository GitHub.
2. GitHub Actions parte da solo ("Build Magneto").
3. A compilazione finita, nella pagina dell'esecuzione trovi gli artifact:
   - `Magneto-VST3-Windows` → copia `Magneto.vst3` in `C:\Program Files\Common Files\VST3`
   - `Magneto-VST3-AU-macOS` → VST3 in `~/Library/Audio/Plug-Ins/VST3`, AU in `~/Library/Audio/Plug-Ins/Components`

## Comandi

| Comando | Funzione |
|---|---|
| ON / OFF | Accende e spegne il plugin (bypass totale senza click). Si spengono anche il LED rosso e la luce del VU. |
| DRIVE | Livello di registrazione sul nastro: più alto = nastro più spinto, saturo e compresso. |
| DRY/WET | Miscela tra segnale pulito ed eco (0 = solo dry, 10 = solo eco), a potenza costante. |
| DELAY TIME | Da 40 ms a 2 s. Cambiandolo il nastro "scivola" e l'altezza si piega, come su una macchina vera. |
| TAP TEMPO | Premilo per mettere il delay a tempo con il progetto (il LED lampeggia a tempo); premilo di nuovo per tornare ai millisecondi. Fa la stessa cosa della scritta MIDI CLOCK / MANUAL. |
| REPEAT | Numero di ripetizioni. Oltre il 9 il nastro va in auto-oscillazione (controllata). Con DRIVE alto le ripetizioni restano in piedi più a lungo. |
| MODE 1-4 | Tipo di nastro: 1 Nuovo, 2 Studio (EP-3), 3 Usurato (RE-201), 4 Morente (wow forte, buio, dropout). |
| MIDI CLOCK / MANUAL | Clicca la scritta: MIDI CLOCK sincronizza al BPM della DAW: il DELAY TIME scatta solo su 1/16T, 1/16, 1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/4D (T = terzina, D = puntato) e la suddivisione appare su una targhetta sopra la manopola, MANUAL usa i millisecondi. |
| OUTPUT | Volume finale. A 5 = 0 dB, a 10 = +12 dB. |
| INPUT 1 / INPUT 2 | Clicca il jack: INPUT 1 = eco mono classico, INPUT 2 = eco stereo. Il jack attivo ha l'anello ambra. |
| HISS | Fruscio del nastro: 0 = silenzio, 3 = livello di default (cambia con il MODE), 10 = circa +18 dB. Si accumula con le ripetizioni e si sente solo nella parte wet. |
| BYPASS | Clicca il jack: smette di registrare sul nastro ma lascia finire le ripetizioni (anello rosso quando attivo). |
| VU | Ago funzionante sul livello d'uscita (0 VU = -18 dBFS, balistica VU da 300 ms). A plugin spento l'ago torna a riposo e la luce si abbassa. |
| Vano del nastro | Animato: bobina, capstan e rulli girano e il nastro scorre. DELAY TIME cambia la velocità (delay corto = nastro veloce), MODE cambia l'oscillazione (wow) e sui nastri 3 e 4 il meccanismo trema. Allo spegnimento rallenta e si ferma. |

Manopole: trascina su/giù, Shift per la regolazione fine, doppio clic per il valore di default, rotellina del mouse.
La finestra si ridimensiona dall'angolo in basso a destra.

## Note tecniche

- Framework: JUCE 8 (scaricato automaticamente da CMake).
- Motore audio scritto da zero (`Source/TapeEcho.h`), ispirato all'approccio dei tape delay Airwindows:
  testina di lettura modulata (wow, flutter, deriva casuale), saturazione asimmetrica, filtri nel loop di feedback,
  head bump, fruscio e dropout sul nastro 4.
