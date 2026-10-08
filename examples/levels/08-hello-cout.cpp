// The standard library is always there: print with std::cout.
#include <iostream>
#include <string>
using namespace std;

string greeting = "Hello";
int harvested = 0;
for (int i = 0; i < 3; i++) {
    if (harvest()) {
        harvested++;
    }
    move(East);
}
cout << greeting << ", farmer! Harvested: " << harvested << endl;
