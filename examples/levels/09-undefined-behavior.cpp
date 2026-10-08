// Off by one: the last iteration writes outside the array.
int rows[5];
for (int i = 0; i <= 5; i++) {
    rows[i] = i * 10;
    move(East);
}
