
Pour executer le code veuillez suivre les étapes suivantes :

- se rendre dans le répertoire : autotune_project\rtaudio-6.0.1\tests

- compiler le code avec la commande :
g++ -Wall -D__WINDOWS_WASAPI__ -Iinclude -o duplex duplex.cpp RtAudio.cpp -lole32 -lwinmm -lksuser -lmfplat -lmfuuid -lwmcodecdspuuid

- executer le code avec : .\duplex.exe 1 44100    
- appuyer sur entrer pour stoper l'execution
- Executer le code plot_dump pour créer les audio en .wav et visualiser les signaux.