#!/usr/bin/env python3
"""Italian opening names for Pragma Chess.

Reads lichess-openings.tsv (lichess-org/chess-openings, CC0) and writes
lichess-openings-it.tsv with the same ECO codes and moves and the names in
Italian, as Italian chess books write them: the kind of opening first
("Difesa Siciliana", "Gambetto di Donna Rifiutato"), then its variations
("Variante Najdorf", "Variante di Cambio", "Linea principale").

The translation is a hand-written dictionary (families, kinds of line,
adjectives with their gender, common nouns) plus word-order rules; every word
that is not in it is a proper name and is kept as it is. Moves inside names
use Italian letters (Ad3, Cf3).

    python3 make-italian-names.py            # writes lichess-openings-it.tsv
    python3 make-italian-names.py --unknown  # lists capitalized words kept verbatim
"""

import csv
import re
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent

# The opening families: the part before ':'.
FAMILIES = {
    "Alekhine Defense": "Difesa Alekhine",
    "Amar Opening": "Apertura Amar",
    "Amazon Attack": "Attacco dell'Amazzone",
    "Amsterdam Attack": "Attacco di Amsterdam",
    "Anderssen's Opening": "Apertura Anderssen",
    "Australian Defense": "Difesa Australiana",
    "Barnes Defense": "Difesa Barnes",
    "Barnes Opening": "Apertura Barnes",
    "Basque Opening": "Apertura Basca",
    "Benko Gambit": "Gambetto Benko",
    "Benko Gambit Accepted": "Gambetto Benko Accettato",
    "Benko Gambit Declined": "Gambetto Benko Rifiutato",
    "Benoni Defense": "Difesa Benoni",
    "Bird Opening": "Apertura Bird",
    "Bishop's Opening": "Apertura d'Alfiere",
    "Blackmar-Diemer Gambit": "Gambetto Blackmar-Diemer",
    "Blackmar-Diemer Gambit Accepted": "Gambetto Blackmar-Diemer Accettato",
    "Blackmar-Diemer Gambit Declined": "Gambetto Blackmar-Diemer Rifiutato",
    "Blumenfeld Countergambit": "Controgambetto Blumenfeld",
    "Blumenfeld Countergambit Accepted": "Controgambetto Blumenfeld Accettato",
    "Bogo-Indian Defense": "Difesa Bogo-Indiana",
    "Bongcloud Attack": "Attacco Bongcloud",
    "Borg Defense": "Difesa Borg",
    "Canard Opening": "Apertura Canard",
    "Caro-Kann Defense": "Difesa Caro-Kann",
    "Carr Defense": "Difesa Carr",
    "Catalan Opening": "Apertura Catalana",
    "Center Game": "Partita del Centro",
    "Center Game Accepted": "Partita del Centro Accettata",
    "Clemenz Opening": "Apertura Clemenz",
    "Colle System": "Sistema Colle",
    "Creepy Crawly Formation": "Formazione Creepy Crawly",
    "Czech Defense": "Difesa Ceca",
    "Danish Gambit": "Gambetto Danese",
    "Danish Gambit Accepted": "Gambetto Danese Accettato",
    "Danish Gambit Declined": "Gambetto Danese Rifiutato",
    "Dresden Opening": "Apertura di Dresda",
    "Duras Gambit": "Gambetto Duras",
    "Dutch Defense": "Difesa Olandese",
    "Döry Defense": "Difesa Döry",
    "East Indian Defense": "Difesa Est-Indiana",
    "Elephant Gambit": "Gambetto dell'Elefante",
    "English Defense": "Difesa Inglese",
    "English Opening": "Apertura Inglese",
    "English Orangutan": "Orangutan Inglese",
    "Englund Gambit": "Gambetto Englund",
    "Englund Gambit Declined": "Gambetto Englund Rifiutato",
    "Formation": "Formazione",
    "Four Knights Game": "Partita dei Quattro Cavalli",
    "French Defense": "Difesa Francese",
    "Fried Fox Defense": "Difesa della Volpe Fritta",
    "Global Opening": "Apertura Globale",
    "Goldsmith Defense": "Difesa Goldsmith",
    "Grob Opening": "Apertura Grob",
    "Grünfeld Defense": "Difesa Grünfeld",
    "Gunderam Defense": "Difesa Gunderam",
    "Hippopotamus Defense": "Difesa dell'Ippopotamo",
    "Horwitz Defense": "Difesa Horwitz",
    "Hungarian Opening": "Apertura Ungherese",
    "Indian Defense": "Difesa Indiana",
    "Irish Gambit": "Gambetto Irlandese",
    "Italian Game": "Partita Italiana",
    "Kangaroo Defense": "Difesa del Canguro",
    "King's Gambit": "Gambetto di Re",
    "King's Gambit Accepted": "Gambetto di Re Accettato",
    "King's Gambit Declined": "Gambetto di Re Rifiutato",
    "King's Indian Attack": "Attacco Indiano di Re",
    "King's Indian Attack, with Bf5": "Attacco Indiano di Re, con Af5",
    "King's Indian Attack, with e6": "Attacco Indiano di Re, con e6",
    "King's Indian Defense": "Difesa Indiana di Re",
    "King's Knight Opening": "Apertura di Cavallo di Re",
    "King's Pawn Game": "Partita di Pedone di Re",
    "King's Pawn Opening": "Apertura di Pedone di Re",
    "Kádas Opening": "Apertura Kádas",
    "Lasker Simul Special": "Speciale da simultanea di Lasker",
    "Latvian Gambit": "Gambetto Lettone",
    "Latvian Gambit Accepted": "Gambetto Lettone Accettato",
    "Lemming Defense": "Difesa del Lemming",
    "Lion Defense": "Difesa del Leone",
    "London System": "Sistema di Londra",
    "London System, with Bd3": "Sistema di Londra, con Ad3",
    "London System, with Be2": "Sistema di Londra, con Ae2",
    "Marienbad System": "Sistema Marienbad",
    "Mexican Defense": "Difesa Messicana",
    "Mieses Opening": "Apertura Mieses",
    "Mikenas Defense": "Difesa Mikenas",
    "Modern Defense": "Difesa Moderna",
    "Montevideo Defense": "Difesa Montevideo",
    "Neo-Grünfeld Defense": "Difesa Neo-Grünfeld",
    "Nimzo-Indian Defense": "Difesa Nimzo-Indiana",
    "Nimzo-Larsen Attack": "Attacco Nimzo-Larsen",
    "Nimzowitsch Defense": "Difesa Nimzowitsch",
    "Old Indian Defense": "Difesa Vecchia Indiana",
    "Owen Defense": "Difesa Owen",
    "Paleface Attack": "Attacco Viso Pallido",
    "Petrov's Defense": "Difesa Russa",
    "Philidor Defense": "Difesa Philidor",
    "Pirc Defense": "Difesa Pirc",
    "Polish Defense": "Difesa Polacca",
    "Polish Opening": "Apertura Polacca",
    "Polish Opening, with d5": "Apertura Polacca, con d5",
    "Ponziani Opening": "Apertura Ponziani",
    "Portuguese Opening": "Apertura Portoghese",
    "Pseudo Queen's Indian Defense": "Difesa Pseudo-Indiana di Donna",
    "Pterodactyl Defense": "Difesa dello Pterodattilo",
    "Queen's Gambit": "Gambetto di Donna",
    "Queen's Gambit Accepted": "Gambetto di Donna Accettato",
    "Queen's Gambit Declined": "Gambetto di Donna Rifiutato",
    "Queen's Indian Accelerated": "Indiana di Donna Accelerata",
    "Queen's Indian Defense": "Difesa Indiana di Donna",
    "Queen's Indian Defense, with e3": "Difesa Indiana di Donna, con e3",
    "Queen's Indian Defense, with e3, Bb4+ Line": "Difesa Indiana di Donna, con e3, Linea Ab4+",
    "Queen's Pawn Game": "Partita di Pedone di Donna",
    "Queen's Pawn, Mengarini Attack": "Pedone di Donna, Attacco Mengarini",
    "Rapport-Jobava System": "Sistema Rapport-Jobava",
    "Rapport-Jobava System, with e6": "Sistema Rapport-Jobava, con e6",
    "Rat Defense": "Difesa del Ratto",
    "Richter-Veresov Attack": "Attacco Richter-Veresov",
    "Robatsch Defense": "Difesa Robatsch",
    "Rubinstein Opening": "Apertura Rubinstein",
    "Ruy Lopez": "Partita Spagnola",
    "Réti Opening": "Apertura Réti",
    "Saragossa Opening": "Apertura di Saragozza",
    "Scandinavian Defense": "Difesa Scandinava",
    "Scotch Game": "Partita Scozzese",
    "Semi-Slav Defense": "Difesa Semi-Slava",
    "Semi-Slav Defense Accepted": "Difesa Semi-Slava Accettata",
    "Sicilian Defense": "Difesa Siciliana",
    "Slav Defense": "Difesa Slava",
    "Slav Indian": "Slava Indiana",
    "Sodium Attack": "Attacco del Sodio",
    "St. George Defense": "Difesa San Giorgio",
    "Tarrasch Defense": "Difesa Tarrasch",
    "Three Knights Opening": "Partita dei Tre Cavalli",
    "Torre Attack": "Attacco Torre",
    "Trompowsky Attack": "Attacco Trompowsky",
    "Valencia Opening": "Apertura di Valencia",
    "Van Geet Opening": "Apertura Van Geet",
    "Van't Kruijs Opening": "Apertura Van't Kruijs",
    "Vienna Gambit, with Max Lange Defense": "Gambetto Viennese, con la Difesa Max Lange",
    "Vienna Game": "Partita Viennese",
    "Vulture Defense": "Difesa dell'Avvoltoio",
    "Wade Defense": "Difesa Wade",
    "Ware Defense": "Difesa Ware",
    "Ware Opening": "Apertura Ware",
    "Yusupov-Rubinstein System": "Sistema Yusupov-Rubinstein",
    "Zaire Defense": "Difesa Zaire",
    "Zukertort Defense": "Difesa Zukertort",
    "Zukertort Opening": "Apertura Zukertort",
}

# Whole variation parts whose translation is not word by word.
PHRASES = {
    "Main Line": "Linea principale",
    "Main Lines": "Linee principali",
    "Exchange Variation": "Variante di Cambio",
    "Advance Variation": "Variante d'Avanzata",
    "Fried Liver Attack": "Attacco Fegatello",
    "Fegatello Attack": "Attacco Fegatello",
    "Giuoco Piano": "Giuoco Piano",
    "Giuoco Pianissimo": "Giuoco Pianissimo",
    "Two Knights Defense": "Difesa dei Due Cavalli",
    "Poisoned Pawn Variation": "Variante del Pedone Avvelenato",
    "Poisoned Pawn": "Pedone Avvelenato",
    "Accelerated Dragon": "Dragone Accelerato",
    "Hyperaccelerated Dragon": "Dragone Iperaccelerato",
    "Dragon Variation": "Variante del Dragone",
    "Yugoslav Attack": "Attacco Jugoslavo",
    "English Attack": "Attacco Inglese",
    "Four Pawns Attack": "Attacco dei Quattro Pedoni",
    "Two Knights Variation": "Variante dei Due Cavalli",
    "Four Knights Variation": "Variante dei Quattro Cavalli",
    "Three Knights Variation": "Variante dei Tre Cavalli",
    "Scholar's Mate": "Matto del Barbiere",
    "Fool's Mate": "Matto dell'Imbecille",
    "Noah's Ark Trap": "Trappola dell'Arca di Noè",
    "Lasker Trap": "Trappola Lasker",
    "Lasker Defense": "Difesa Lasker",
    "Wing Gambit": "Gambetto dell'Ala",
    "Smith-Morra Gambit": "Gambetto Smith-Morra",
    "Maróczy Bind": "Morsa Maróczy",
    "Hedgehog Variation": "Variante del Riccio",
    "Stonewall Attack": "Attacco Stonewall",
    "Stonewall Variation": "Variante Stonewall",
    "Stonewall": "Stonewall",
    "Closed": "Chiusa",
    "Open": "Aperta",
    "Classical": "Classica",
    "Modern": "Moderna",
    "Normal Variation": "Variante Normale",
    "Rare Defenses": "Difese Rare",
    "Rare Lines": "Linee rare",
    "Long Castling Variation": "Variante dell'Arrocco lungo",
    "Short Castling Variation": "Variante dell'Arrocco corto",
    "Queenside Castling Line": "Linea dell'Arrocco lungo",
    "King's Indian Variation": "Variante Indiana di Re",
    "Queen's Indian Variation": "Variante Indiana di Donna",
    "Queen's Gambit Declined": "Gambetto di Donna Rifiutato",
    "Queen's Gambit Accepted": "Gambetto di Donna Accettato",
    "King's Gambit": "Gambetto di Re",
    "Bishop's Gambit": "Gambetto dell'Alfiere",
    "Knight's Gambit": "Gambetto del Cavallo",
    "King's Knight's Gambit": "Gambetto del Cavallo di Re",
    "Queen's Knight Variation": "Variante del Cavallo di Donna",
    "King's Knight Variation": "Variante del Cavallo di Re",
    "Anti-Marshall": "Anti-Marshall",
    "Marshall Attack": "Attacco Marshall",
    "Berlin Defense": "Difesa Berlinese",
    "Berlin Wall": "Muro di Berlino",
    "Morphy Defense": "Difesa Morphy",
    "Open Variation": "Variante Aperta",
    "Closed Variation": "Variante Chiusa",
    "Mar del Plata Variation": "Variante Mar del Plata",
    "Rio de Janeiro Variation": "Variante Rio de Janeiro",
    "Buenos Aires Variation": "Variante Buenos Aires",
    "Monte Carlo Variation": "Variante Monte Carlo",
    "Grand Prix Attack": "Attacco Grand Prix",
    "St. Petersburg Variation": "Variante di San Pietroburgo",
    "St. Petersburg Gambit": "Gambetto di San Pietroburgo",
    "Old Steinitz Defense": "Vecchia Difesa Steinitz",
    "Modern Steinitz Defense": "Difesa Steinitz Moderna",
    "Steinitz Defense Deferred": "Difesa Steinitz Differita",
    "Fianchetto Defense Deferred": "Difesa del Fianchetto Differita",
    "Irregular Variation": "Variante Irregolare",
    "Drunken Cavalry Variation": "Variante della Cavalleria Ubriaca",
    "Delayed .. Nc6": "..Cc6 Ritardato",
    "Paulsen Attack Variation": "Variante d'Attacco Paulsen",
    "Spassky System3": "Sistema Spassky 3",
    "Two Knights": "Due Cavalli",
    "Sicilian Two Knights": "Siciliana dei Due Cavalli",
    "Three Knights": "Tre Cavalli",
    "Four Knights": "Quattro Cavalli",
}

# Kinds of line: (Italian, gender) — gender drives adjective agreement.
KINDS = {
    "Variation": ("Variante", "f"), "Variations": ("Varianti", "fp"),
    "Gambit": ("Gambetto", "m"), "Countergambit": ("Controgambetto", "m"),
    "Defense": ("Difesa", "f"), "Defenses": ("Difese", "fp"),
    "Attack": ("Attacco", "m"), "Counterattack": ("Contrattacco", "m"),
    "Line": ("Linea", "f"), "Lines": ("Linee", "fp"),
    "System": ("Sistema", "m"), "Formation": ("Formazione", "f"),
    "Trap": ("Trappola", "f"), "Opening": ("Apertura", "f"),
    "Game": ("Partita", "f"), "Deviations": ("Deviazioni", "fp"),
    "Sacrifice": ("Sacrificio", "m"), "Mate": ("Matto", "m"),
    "Invitation": ("Invito", "m"), "Transfer": ("Trasferimento", "m"),
    "Plan": ("Piano", "m"), "Offer": ("Offerta", "f"), "Hybrid": ("Ibrido", "m"),
    "Check": ("Scacco", "m"), "Retreat": ("Ritirata", "f"), "Bind": ("Morsa", "f"),
    "Endgame": ("Finale", "m"), "Push": ("Spinta", "f"), "Break": ("Rottura", "f"),
    "Storm": ("Assalto", "m"), "Pin": ("Inchiodatura", "f"),
    "Connection": ("Collegamento", "m"), "Extension": ("Estensione", "f"),
    "Refutation": ("Confutazione", "f"), "Symmetry": ("Simmetria", "f"),
    "Castling": ("Arrocco", "m"), "Intermezzo": ("Intermezzo", "m"),
    "Battery": ("Batteria", "f"), "Blockade": ("Blocco", "m"),
    "Setup": ("Schieramento", "m"), "Structure": ("Struttura", "f"),
    "Folly": ("Follia", "f"), "Order": ("Ordine", "m"),
    "Counterthrust": ("Contrattacco", "m"), "Development": ("Sviluppo", "m"),
    "Wall": ("Muro", "m"), "Special": ("Speciale", "m"),
    "Fianchetto": ("Fianchetto", "m"),
}

# Words after the kind that agree with it.
MODIFIERS = {
    "Accepted": ("Accettato", "Accettata"),
    "Declined": ("Rifiutato", "Rifiutata"),
    "Deferred": ("Differito", "Differita"),
    "Reversed": ("Invertito", "Invertita"),
    "Refused": ("Rifiutato", "Rifiutata"),
}

# Adjectives: (masculine, feminine). Placed after the kind, before complements.
ADJECTIVES = {
    "Classical": ("Classico", "Classica"), "Modern": ("Moderno", "Moderna"),
    "Closed": ("Chiuso", "Chiusa"), "Open": ("Aperto", "Aperta"),
    "Normal": ("Normale", "Normale"), "Main": ("Principale", "Principale"),
    "Orthodox": ("Ortodosso", "Ortodossa"), "Symmetrical": ("Simmetrico", "Simmetrica"),
    "Symmetric": ("Simmetrico", "Simmetrica"), "Ultra-Symmetrical": ("Ultra-Simmetrico", "Ultra-Simmetrica"),
    "Accelerated": ("Accelerato", "Accelerata"), "Hyperaccelerated": ("Iperaccelerato", "Iperaccelerata"),
    "Quiet": ("Tranquillo", "Tranquilla"), "Traditional": ("Tradizionale", "Tradizionale"),
    "Positional": ("Posizionale", "Posizionale"), "Central": ("Centrale", "Centrale"),
    "Delayed": ("Ritardato", "Ritardata"), "Ultra-Delayed": ("Ultra-Ritardato", "Ultra-Ritardata"),
    "Old": ("Vecchio", "Vecchia"), "New": ("Nuovo", "Nuova"), "Early": ("Anticipato", "Anticipata"),
    "Double": ("Doppio", "Doppia"), "Reversed": ("Invertito", "Invertita"),
    "Defensive": ("Difensivo", "Difensiva"), "Semi-Classical": ("Semi-Classico", "Semi-Classica"),
    "Neo-Classical": ("Neoclassico", "Neoclassica"), "Anti-Classical": ("Anticlassico", "Anticlassica"),
    "Neo-Modern": ("Neo-Moderno", "Neo-Moderna"), "Neo-Orthodox": ("Neo-Ortodosso", "Neo-Ortodossa"),
    "Flexible": ("Flessibile", "Flessibile"), "Full": ("Completo", "Completa"),
    "Fully": ("Completamente", "Completamente"), "Standard": ("Standard", "Standard"),
    "Compromised": ("Compromesso", "Compromessa"), "Sharp": ("Tagliente", "Tagliente"),
    "Improved": ("Migliorato", "Migliorata"), "Original": ("Originale", "Originale"),
    "Rare": ("Raro", "Rara"), "Lesser": ("Minore", "Minore"), "Great": ("Grande", "Grande"),
    "Big": ("Grande", "Grande"), "Small": ("Piccolo", "Piccola"), "Short": ("Corto", "Corta"),
    "Long": ("Lungo", "Lunga"), "Slow": ("Lento", "Lenta"), "Wild": ("Selvaggio", "Selvaggia"),
    "Primitive": ("Primitivo", "Primitiva"), "Forgotten": ("Dimenticato", "Dimenticata"),
    "Immediate": ("Immediato", "Immediata"), "Dynamic": ("Dinamico", "Dinamica"),
    "Fluid": ("Fluido", "Fluida"), "Forcing": ("Forzante", "Forzante"),
    "Transpositional": ("Di trasposizione", "Di trasposizione"), "Extended": ("Esteso", "Estesa"),
    "Inverted": ("Invertito", "Invertita"), "Provincial": ("Provinciale", "Provinciale"),
    "Aged": ("Invecchiato", "Invecchiata"), "Drunken": ("Ubriaco", "Ubriaca"),
    "Wayward": ("Ribelle", "Ribelle"), "Shy": ("Timido", "Timida"), "Mad": ("Pazzo", "Pazza"),
    "Poisoned": ("Avvelenato", "Avvelenata"), "Irregular": ("Irregolare", "Irregolare"),
    "Second": ("Secondo", "Seconda"), "First": ("Primo", "Prima"),
    "Western": ("Occidentale", "Occidentale"), "Eastern": ("Orientale", "Orientale"),
    "West": ("Ovest", "Ovest"), "Inner": ("Interno", "Interna"), "Prickly": ("Spinoso", "Spinosa"),
    "Wimpy": ("Fiacco", "Fiacca"), "Bouncing": ("Rimbalzante", "Rimbalzante"),
    "Storming": ("D'assalto", "D'assalto"), "Harmonist": ("Armonista", "Armonista"),
    "Anti-Modern": ("Antimoderno", "Antimoderna"),
    # Places and peoples.
    "English": ("Inglese", "Inglese"), "Spanish": ("Spagnolo", "Spagnola"),
    "Italian": ("Italiano", "Italiana"), "French": ("Francese", "Francese"),
    "Russian": ("Russo", "Russa"), "Hungarian": ("Ungherese", "Ungherese"),
    "Yugoslav": ("Jugoslavo", "Jugoslava"), "Austrian": ("Austriaco", "Austriaca"),
    "Polish": ("Polacco", "Polacca"), "Dutch": ("Olandese", "Olandese"),
    "Czech": ("Ceco", "Ceca"), "Portuguese": ("Portoghese", "Portoghese"),
    "Scandinavian": ("Scandinavo", "Scandinava"), "Sicilian": ("Siciliano", "Siciliana"),
    "Scotch": ("Scozzese", "Scozzese"), "Indian": ("Indiano", "Indiana"),
    "Norwegian": ("Norvegese", "Norvegese"), "Swiss": ("Svizzero", "Svizzera"),
    "Siberian": ("Siberiano", "Siberiana"), "American": ("Americano", "Americana"),
    "Chinese": ("Cinese", "Cinese"), "Ukrainian": ("Ucraino", "Ucraina"),
    "Argentine": ("Argentino", "Argentina"), "Argentinian": ("Argentino", "Argentina"),
    "Swedish": ("Svedese", "Svedese"), "Finnish": ("Finlandese", "Finlandese"),
    "German": ("Tedesco", "Tedesca"), "Romanian": ("Rumeno", "Rumena"),
    "Danish": ("Danese", "Danese"), "Irish": ("Irlandese", "Irlandese"),
    "Icelandic": ("Islandese", "Islandese"), "Armenian": ("Armeno", "Armena"),
    "Bavarian": ("Bavarese", "Bavarese"), "Bulgarian": ("Bulgaro", "Bulgara"),
    "Lithuanian": ("Lituano", "Lituana"), "Mexican": ("Messicano", "Messicana"),
    "Australian": ("Australiano", "Australiana"), "Maltese": ("Maltese", "Maltese"),
    "Mediterranean": ("Mediterraneo", "Mediterranea"), "Florentine": ("Fiorentino", "Fiorentina"),
    "Viennese": ("Viennese", "Viennese"), "Valencian": ("Valenciano", "Valenciana"),
    "Westphalian": ("Vestfaliano", "Vestfaliana"), "Kazakh": ("Kazako", "Kazaka"),
    "Martian": ("Marziano", "Marziana"), "Alien": ("Alieno", "Aliena"),
    "Baltic": ("Baltico", "Baltica"), "Arctic": ("Artico", "Artica"),
    "Catalan": ("Catalano", "Catalana"), "Slav": ("Slavo", "Slava"),
    "Semi-Slav": ("Semi-Slavo", "Semi-Slava"), "Anglo-Slav": ("Anglo-Slavo", "Anglo-Slava"),
    "Anglo-Indian": ("Anglo-Indiano", "Anglo-Indiana"), "Anglo-Dutch": ("Anglo-Olandese", "Anglo-Olandese"),
    "Anglo-Scandinavian": ("Anglo-Scandinavo", "Anglo-Scandinava"),
    "Anglo-Lithuanian": ("Anglo-Lituano", "Anglo-Lituana"), "Anglo-Grünfeld": ("Anglo-Grünfeld", "Anglo-Grünfeld"),
    "Franco-Sicilian": ("Franco-Siciliano", "Franco-Siciliana"), "Nimzo-English": ("Nimzo-Inglese", "Nimzo-Inglese"),
    "Nimzo-Dutch": ("Nimzo-Olandese", "Nimzo-Olandese"), "Nimzo-American": ("Nimzo-Americano", "Nimzo-Americana"),
    "Czech-Indian": ("Ceco-Indiano", "Ceco-Indiana"), "Double-Dutch": ("Doppio Olandese", "Doppia Olandese"),
    "Neo-Catalan": ("Neo-Catalano", "Neo-Catalana"), "Pseudo-Catalan": ("Pseudo-Catalano", "Pseudo-Catalana"),
    "Pseudo-Spanish": ("Pseudo-Spagnolo", "Pseudo-Spagnola"), "Pseudo-Slav": ("Pseudo-Slavo", "Pseudo-Slava"),
    "Pseudo-Austrian": ("Pseudo-Austriaco", "Pseudo-Austriaca"),
    "Pseudo-Scandinavian": ("Pseudo-Scandinavo", "Pseudo-Scandinava"),
    "Batavo-Polish": ("Batavo-Polacco", "Batavo-Polacca"),
}

# Nouns that become complements ("di Cambio", "del Dragone"), placed last.
COMPLEMENTS = {
    "Exchange": "di Cambio", "Advance": "d'Avanzata", "Fianchetto": "del Fianchetto",
    "Dragon": "del Dragone", "Hedgehog": "del Riccio", "Center": "del Centro",
    "Wing": "dell'Ala", "Kingside": "sul lato di Re", "Queenside": "sul lato di Donna",
    "Pawn": "di Pedone", "Pawns": "dei Pedoni", "Knight": "di Cavallo", "Knights": "dei Cavalli",
    "Bishop": "d'Alfiere", "Bishop's": "dell'Alfiere", "Knight's": "del Cavallo",
    "King's": "di Re", "Queen's": "di Donna", "King": "di Re", "Queen": "di Donna",
    "Rooks": "delle Torri", "Two": "Due", "Three": "Tre", "Four": "Quattro", "Six": "Sei",
    "Retreat": "della Ritirata", "Castling": "dell'Arrocco", "Check": "dello Scacco",
    "Endgame": "del Finale", "Symmetry": "della Simmetria", "Move": "di Mossa",
    "Development": "di Sviluppo", "Correspondence": "per Corrispondenza", "Simul": "da Simultanea",
    "Pin": "dell'Inchiodatura", "Bind": "della Morsa", "Blockade": "del Blocco",
    "Spike": "della Punta", "Bayonet": "della Baionetta", "Beginner's": "del Principiante",
    "Refutation": "della Confutazione", "Offer": "dell'Offerta", "Push": "della Spinta",
    "Storm": "dell'Assalto", "Hunt": "della Caccia", "Edge": "del Bordo", "Head": "della Testa",
    "Break": "della Rottura", "Plan": "del Piano", "Setup": "dello Schieramento",
    # Animals and things.
    "Pterodactyl": "dello Pterodattilo", "Pteranodon": "del Pteranodonte",
    "Rhamphorhynchus": "del Ramforinco", "Quetzalcoatlus": "del Quetzalcoatlo",
    "Siroccopteryx": "del Siroccopteryx", "Austriadactylus": "dell'Austriadattilo",
    "Hippopotamus": "dell'Ippopotamo", "Rat": "del Ratto", "Lion": "del Leone", "Lion's": "del Leone",
    "Snake": "del Serpente", "Lizard": "della Lucertola", "Crab": "del Granchio", "Duck": "dell'Anatra",
    "Walrus": "del Tricheco", "Penguin": "del Pinguino", "Porcupine": "del Porcospino",
    "Mosquito": "della Zanzara", "Whale": "della Balena", "Wolf": "del Lupo", "Hawk": "del Falco",
    "Falcon": "del Falco", "Snail": "della Lumaca", "Squirrel": "dello Scoiattolo",
    "Tortoise": "della Tartaruga", "Giraffe": "della Giraffa", "Crocodile": "del Coccodrillo",
    "Wasp": "della Vespa", "Lobster": "dell'Aragosta", "Cobra": "del Cobra", "Mongoose": "della Mangusta",
    "Unicorn": "dell'Unicorno", "Goblin": "del Folletto", "Monster": "del Mostro",
    "Cormorant": "del Cormorano", "Kingfisher": "del Martin pescatore", "Horsefly": "del Tafano",
    "Gibbon": "del Gibbone", "Raptor": "del Rapace", "Woodchuck": "della Marmotta", "Dodo": "del Dodo",
    "Chameleon": "del Camaleonte", "Beefeater": "del Beefeater", "Hammer": "del Martello",
    "Clamp": "del Morsetto", "Claw": "dell'Artiglio", "Clam": "della Vongola", "Jaw": "della Mascella",
    "Elbow": "del Gomito", "Liver": "del Fegato", "Potato": "della Patata", "Cheese": "del Formaggio",
    "Toilet": "del Gabinetto", "Beach": "della Spiaggia", "Cave": "della Caverna", "Brick": "del Mattone",
    "Doll": "della Bambola", "Drill": "del Trapano", "Whip": "della Frusta", "Corkscrew": "del Cavatappi",
    "Mustang": "del Mustang", "Cavalry": "della Cavalleria", "Scorpion": "dello Scorpione",
    "Nightingale": "dell'Usignolo", "Tumbleweed": "del Cespuglio rotolante", "Wind": "del Vento",
    "Apocalypse": "dell'Apocalisse", "Millennium": "del Millennio", "Halloween": "di Halloween",
    "Dream": "del Sogno", "Gossip": "del Pettegolezzo", "Siesta": "della Siesta", "Mafia": "della Mafia",
    "Bum": "del Sedere", "Monkey's": "della Scimmia", "Fishing": "della Pesca", "Pole": "della Canna",
    "Tour": "del Giro", "Walk": "della Passeggiata", "Return": "del Ritorno", "Pass": "del Passo",
    "Swap": "dello Scambio", "Outflank": "dell'Aggiramento", "Unpin": "della Schiodatura",
    "Grab": "della Presa", "Shuffle": "del Rimescolamento", "Banker": "del Banchiere",
    "Harksen": "Harksen", "Dog": "del Cane", "Fox": "della Volpe",
    "Snagglepuss": "di Snagglepuss", "Picklepuss": "di Picklepuss", "Thunderbunny": "di Thunderbunny",
    "Kiddie": "dei Bambini", "Oldtimer": "del Veterano", "Plasma": "del Plasma",
    "Meadow": "del Prato", "Hay": "del Fieno", "Omega": "Omega",
}

# Places whose Italian name differs; the rest are kept.
PLACES = {
    "Berlin": "Berlinese", "Vienna": "Viennese", "Moscow": "di Mosca", "London": "di Londra",
    "Paris": "di Parigi", "Prague": "di Praga", "Warsaw": "di Varsavia", "Venice": "di Venezia",
    "Zurich": "di Zurigo", "Copenhagen": "di Copenaghen", "Cologne": "di Colonia",
    "Brussels": "di Bruxelles", "Lisbon": "di Lisbona", "Belgrade": "di Belgrado",
    "Budapest": "di Budapest", "Leningrad": "di Leningrado", "Stockholm": "di Stoccolma",
    "Cambridge": "di Cambridge", "Oxford": "di Oxford", "Hastings": "di Hastings",
    "Carlsbad": "di Karlsbad", "Meran": "di Merano", "Abbazia": "di Abbazia", "Dresden": "di Dresda",
    "Frankfurt": "di Francoforte", "Nürnberg": "di Norimberga", "Munich": "di Monaco",
    "Breslau": "di Breslavia", "Cracow": "di Cracovia", "Seville": "di Siviglia", "Riga": "di Riga",
    "Graz": "di Graz", "Bled": "di Bled", "Zagreb": "di Zagabria", "Wiesbaden": "di Wiesbaden",
    "Mannheim": "di Mannheim", "Aachen": "di Aquisgrana", "Kiel": "di Kiel", "Semmering": "del Semmering",
    "Petersburg": "di Pietroburgo", "Novosibirsk": "di Novosibirsk", "Tashkent": "di Tashkent",
    "Yerevan": "di Erevan", "Chelyabinsk": "di Čeljabinsk", "Arkhangelsk": "di Arcangelo",
    "Neo-Arkhangelsk": "Neo-Arcangelo", "Voronezh": "di Voronež", "Debrecen": "di Debrecen",
    "Kecskemet": "di Kecskemét", "Podebrady": "di Poděbrady", "Edinburgh": "di Edimburgo",
    "Melbourne": "di Melbourne", "Chicago": "di Chicago", "Portland": "di Portland",
    "Birmingham": "di Birmingham", "Brooklyn": "di Brooklyn", "Manhattan": "di Manhattan",
    "Omaha": "di Omaha", "Colorado": "del Colorado", "Massachusetts": "del Massachusetts",
    "Guatemala": "del Guatemala", "Haiti": "di Haiti", "Malvinas": "delle Malvinas",
    "Pyrenees": "dei Pirenei", "Everglades": "delle Everglades", "Eifel": "dell'Eifel",
    "Düsseldorf": "di Düsseldorf", "Tübingen": "di Tubinga", "Bayreuth": "di Bayreuth",
    "Goteborg": "di Göteborg", "Amsterdam": "di Amsterdam", "Beverwijk": "di Beverwijk",
    "Jalalabad": "di Jalalabad", "Battambang": "di Battambang", "Eastbourne": "di Eastbourne",
    "Portsmouth": "di Portsmouth", "Romford": "di Romford", "Norfolk": "del Norfolk",
    "Shropshire": "dello Shropshire", "Stafford": "di Stafford", "Troon": "di Troon",
    "Bradford": "di Bradford", "Acton": "di Acton", "Weinsbach": "di Weinsbach",
    "Barmen": "di Barmen", "Zinnowitz": "di Zinnowitz", "Hjørring": "di Hjørring",
    "Herford": "di Herford", "Netherlands": "dei Paesi Bassi", "England": "d'Inghilterra",
    "Benelux": "del Benelux", "Trencianske-Teplice": "di Trenčianske Teplice", "Dudweiler": "di Dudweiler",
    "Winterberg": "di Winterberg",
}

# Small words.
WORDS = {"with": "con", "and": "e", "the": "il", "The": "Il", "de": "de", "del": "del",
         "..": "..", "St.": "San", "Dr.": "Dr."}

# SAN in names in Italian letters: K R, Q D, R T, B A, N C.
SAN = re.compile(r"(?<![A-Za-z])([KQRBN])(?=x?[a-h]?[1-8]?x?[a-h][1-8][+#]?(?![A-Za-z0-9]))")
ITALIAN_PIECE = {"K": "R", "Q": "D", "R": "T", "B": "A", "N": "C"}


def italian_san(text):
    return SAN.sub(lambda m: ITALIAN_PIECE[m.group(1)], text)


unknown = Counter()

# Adjectives that Italian puts before the noun ("Doppio Gambetto", "Vecchia Linea").
BEFORE = {"Double", "Great", "Big", "Small", "Second", "First", "Old", "New", "Lesser"}
# Feminine nouns behind "dell'".
FEMININE_ELIDED = {"Ala", "Anatra", "Aragosta", "Offerta", "Inchiodatura", "Apocalisse", "Artiglio"}
FEMININE_ELIDED.discard("Artiglio")
ST = {"George": "San Giorgio", "Patrick's": "San Patrizio", "Petersburg": "San Pietroburgo"}
NUMBERS = {"Two": "Due", "Three": "Tre", "Four": "Quattro", "Six": "Sei"}
ARTICLES = ("dello ", "della ", "delle ", "degli ", "dei ", "del ", "dell'", "di ", "d'")


def noun_of(complement):
    """"del Dragone" -> ("Dragone", "m"); "della Punta" -> ("Punta", "f")."""
    for article in ARTICLES:
        if complement.startswith(article):
            bare = complement[len(article):]
            if article in ("della ", "delle "):
                gender = "f"
            elif article == "dell'":
                gender = "f" if bare in FEMININE_ELIDED else "m"
            elif article == "d'":
                gender = "f" if bare.endswith("a") else "m"
            else:
                gender = "m" if not bare.endswith("a") or bare in ("Sistema", "Cobra", "Plasma") else "f"
            return bare[:1].upper() + bare[1:], gender
    return complement, "m"


def definite(noun, gender):
    if noun[:1] in "AEIOUaeiou":
        return "L'" + noun
    if gender == "f":
        return "La " + noun
    if noun[:1] in "Zz" or noun[:2].lower() in ("ps", "pt", "gn", "sc", "st", "sp", "sq"):
        return "Lo " + noun
    return "Il " + noun


def hyphenated(word):
    """"Spielmann-Indian" -> "Spielmann-Indiana", "Anti-English" -> "Anti-Inglese"."""
    head, _, last = word.rpartition("-")
    if head and last in ADJECTIVES:
        return head + "-" + ADJECTIVES[last][1]
    return None


def items_of(words, known_kind=None):
    """Classify the words of a part: adjectives, nouns, names, moves."""
    items = []
    i = 0
    while i < len(words):
        w = words[i]
        nxt = words[i + 1] if i + 1 < len(words) else None
        if w == "St." and nxt:
            items.append(("name", ST.get(nxt, "San " + nxt.removesuffix("'s"))))
            i += 2
            continue
        if w in NUMBERS and nxt in COMPLEMENTS:
            bare, gender = noun_of(COMPLEMENTS[nxt])
            article = "dei" if gender == "m" else "delle"
            items.append(("noun", f"{NUMBERS[w]} {bare}", "mp" if gender == "m" else "fp", f"{article} {NUMBERS[w]} {bare}"))
            i += 2
            continue
        if w in ("King's", "Queen's") and nxt == "Indian":
            side = "di Re" if w == "King's" else "di Donna"
            items.append(("adj", ("Indiano " + side, "Indiana " + side), False))
            i += 2
            continue
        if w in ("King's", "Queen's") and nxt in ("Pawn", "Knight", "Bishop", "Rook", "English"):
            side = "di Re" if w == "King's" else "di Donna"
            if nxt == "English":
                items.append(("adj", ("Inglese " + side, "Inglese " + side), False))
            else:
                bare = {"Pawn": "Pedone", "Knight": "Cavallo", "Bishop": "Alfiere", "Rook": "Torre"}[nxt]
                gender = "f" if bare == "Torre" else "m"
                article = {"Pedone": "del", "Cavallo": "del", "Alfiere": "dell'", "Torre": "della"}[bare]
                sep = "" if article.endswith("'") else " "
                items.append(("noun", f"{bare} {side}", gender, f"{article}{sep}{bare} {side}"))
            i += 2
            continue
        if w == "Main":
            items.append(("adj", ("principale", "principale"), False))
        elif w in ADJECTIVES:
            items.append(("adj", ADJECTIVES[w], w in BEFORE))
        elif w in COMPLEMENTS:
            bare, gender = noun_of(COMPLEMENTS[w])
            items.append(("noun", bare, gender, COMPLEMENTS[w]))
        elif w in KINDS and known_kind is not None:
            bare, gender = KINDS[w]
            complement = {"Attack": "d'Attacco", "Gambit": "di Gambetto", "Defense": "di Difesa"}.get(w, "di " + bare)
            items.append(("noun", bare, gender, complement))
        elif w in PLACES:
            place = PLACES[w]
            items.append(("place", place))
        elif w in WORDS:
            items.append(("word", WORDS[w]))
        elif w in MODIFIERS:
            items.append(("modifier", w))
        elif SAN.search(w) or re.fullmatch(r"[a-h][1-8]|[a-h]x[a-h][1-8]|\.\.", w):
            items.append(("move", w))
        elif hyphenated(w):
            items.append(("adj", (hyphenated(w), hyphenated(w)), False))
        else:
            if w[:1].isupper():
                unknown[w] += 1
            if w.endswith("'s") and len(w) > 3:
                items.append(("possessive", w[:-2]))
            else:
                items.append(("name", w))
        i += 1
    return items


def agree(pair, gender):
    return pair[0] if gender.startswith("m") else pair[1]


def assemble(head, gender, items, modifiers):
    """Italian order: [adjectives that go before] head [names] [adjectives] [complements] [modifiers]."""
    before = [agree(it[1], gender) for it in items if it[0] == "adj" and it[2]]
    names = [it[1] for it in items if it[0] in ("name", "move")]
    adjectives = [agree(it[1], gender) for it in items if it[0] == "adj" and not it[2]]
    # "principale" first among the adjectives: "Linea principale Classica".
    adjectives.sort(key=lambda a: a != "principale")
    complements = [it[3] for it in items if it[0] == "noun"]
    complements += [it[1] for it in items if it[0] == "place"]
    complements += ["di " + it[1] for it in items if it[0] == "possessive"]
    words = [it[1] for it in items if it[0] == "word"]
    after = [agree(MODIFIERS[m], gender) for m in modifiers]
    return " ".join(before + [head] + names + adjectives + complements + words + after)


def part(text):
    """One variation part ("Najdorf Variation", "Main Line", "with Bf5")."""
    text = text.strip()
    if text in PHRASES:
        return PHRASES[text]
    if text in FAMILIES:
        return FAMILIES[text]
    words = text.split()
    if words and words[0] == "with":
        rest = " ".join(WORDS.get(w, w) for w in words[1:])
        return "con " + italian_san(rest)
    if words and words[0] in ("The", "the") and len(words) == 2 and words[1] in COMPLEMENTS:
        return definite(*noun_of(COMPLEMENTS[words[1]]))
    modifiers = []
    while words and words[-1] in MODIFIERS:
        modifiers.insert(0, words.pop())
    reversed_colours = False
    if words and words[0] in ("Reversed", "Inverted") and (len(words) == 1 or words[-1] not in KINDS):
        reversed_colours = True
        words = words[1:]

    if len(words) >= 1 and words[-1] in KINDS and (len(words) > 1 or modifiers):
        kind = words.pop()
        head, gender = KINDS[kind]
        result = assemble(head, gender, items_of(words, kind), modifiers)
    elif len(words) == 1 and words[0] in KINDS:
        result = KINDS[words[0]][0]
    else:
        # No kind of line ("Reversed Grünfeld", "Lion's Jaw", "Three Knights"):
        # the last noun or name is the head.
        items = items_of(words)
        if all(it[0] in ("name", "move") for it in items):
            return italian_san(" ".join(words) + (" a colori invertiti" if reversed_colours else ""))
        head_index = next((k for k in range(len(items) - 1, -1, -1)
                           if items[k][0] in ("noun", "name", "adj", "move", "place")), None)
        if head_index is None:
            result = " ".join(it[1] for it in items)
        else:
            head_item = items.pop(head_index)
            if head_item[0] == "noun":
                head, gender = head_item[1], head_item[2]
            elif head_item[0] == "adj":
                head, gender = head_item[1][1], "f"
            elif head_item[0] == "place":
                head, gender = head_item[1], "f"
            elif head_item[0] == "move":
                head, gender = head_item[1], "m"
            else:
                head, gender = head_item[1], "f"
            result = assemble(head, gender, items, modifiers)
    if reversed_colours:
        result += " a colori invertiti"
    return italian_san(result)


def translate(name):
    family, _, rest = name.partition(":")
    it_family = FAMILIES.get(family)
    if it_family is None:
        it_family = part(family)
        print(f"family not in the dictionary: {family}", file=sys.stderr)
    if not rest:
        return it_family
    parts = [part(p) for p in rest.split(",")]
    return it_family + ": " + ", ".join(parts)


def main():
    source = HERE / "lichess-openings.tsv"
    target = HERE / "lichess-openings-it.tsv"
    with source.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f, delimiter="\t"))
    out = [f"eco\tname\tpgn\n"]
    for row in rows:
        out.append(f"{row['eco']}\t{translate(row['name'])}\t{row['pgn']}\n")
    if "--unknown" in sys.argv:
        for word, count in unknown.most_common():
            print(f"{count}\t{word}")
        return
    target.write_text("".join(out), encoding="utf-8")
    print(f"{len(rows)} names written to {target.name}")


if __name__ == "__main__":
    main()
