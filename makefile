FLAGS = -DHAVE_FFSLL


twsearch: twsearch.cpp
	g++ $(FLAGS) -Icityhash/src -O3 -Wextra -Wall -pedantic -std=c++14 -g -march=native -o twsearch twsearch.cpp cityhash/src/city.cc -lpthread
