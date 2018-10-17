twsearch: twsearch.cpp
	g++ -Icityhash/src -O3 -Wextra -Wall -pedantic -std=c++14 -g -march=native -o twsearch twsearch.cpp cityhash/src/city.cc -lpthread
