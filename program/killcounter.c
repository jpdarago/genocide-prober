#include <stdio.h>

int main() {
	FILE * fd = fopen("/dev/killcounter", "r");
	int kills = 0;
	fread(&kills, sizeof(int), 1, fd);
	printf("%d", kills);
	return 0;
}
