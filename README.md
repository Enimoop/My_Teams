# Installations ncurses :
```bash
sudo apt-get update
sudo apt-get install libncurses5-dev libncursesw5-dev
```

# pour compiler le programme client :
```bash
gcc client.c -o client -lncurses
```
# pour kick un client (seulement le premier client connecté peut kick les autres) :
```bash
/kick pseudo
```

# pour changer de status (un seul mot, pas d'espace):
```bash
/status status
```
