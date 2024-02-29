# Installations ncurses :
```bash
sudo apt-get update
sudo apt-get install libncurses5-dev libncursesw5-dev
```

pour compiler le programme client :
```bash
gcc -o client client.c -lncurses
```