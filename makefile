all: main

main: main.cpp
	g++ -lncurses main.cpp -o main
	sudo cp main /usr/bin/tfm
