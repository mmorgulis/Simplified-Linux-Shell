Elaborato dsh.c 
Estensione 2, definizione di variabili

Descrizione: 
Per sviluppare l'estensione che si occupa della definizione di nuove variabili mi sono servito di una mappa di stringhe, utilizzata come variabile globale, con entry del tipo: <NomeVariabile, ValoreVariabile> e con dimensione massima 100 coppie chiave-valore.
Nel main sono state inserite delle condizioni per verificare la presenza del "set" e in questo caso il main chiama il metodo set(token2, token3).
Quest'ultimo prende i due token come parametri e li aggiunge alla mappa oppure li aggiorna se già presenti. 
Inoltre nel main è stato aggiunto il riconoscimento del carattere speciale "$" che si occupa di sostituire una dichiarazione del tipo "$variabile" con "valoreVariabile".


Metodi implementati e modifiche:
Set: void set(const char* nomeVar, const char* valueVar)
Gestisce PATH (non path poichè dsh è case sensitive), in quanto variabile più significativa, aggiungendolo sia nella mappa, sia aggiornando la sua variabile globale associata.
Se il nome della variabile, cioè il token successivo a set, non è NULL allora aggiunge/aggiorna il valore della variabile nella mappa.


Modifiche nel main: 
All'inizio si inizializza la mappa e si aggiunge in prima posizione il path standard.
Nei builtins è stato aggiunto il riconoscimento di "set".
Nei caratteri speciali è stata aggiunta la gestione di "$"
Infine è stata liberata la memoria occupata dalla mappa.


Utilizzo e test:
Creazione e stampa di una nuova variabile:
dsh$ set VAR buongiorno
dsh$ echo $VAR
buongiorno

Altre variabili:
dsh$ set VAR1 foo
dsh$ set VAR2 bar
dsh$ echo $VAR1
foo
dsh$ echo $VAR2
bar
dsh$ set VAR1 foobar
dsh$ echo $VAR1
foobar

Gestione del path:
Path di defualt:
dsh$ echo $PATH
/bin/:/usr/bin/

Nuovo path:
dsh$ set PATH /usr/bin/
dsh$ echo $PATH
/usr/bin/
