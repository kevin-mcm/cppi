// Crops that know how to tend themselves (virtual functions).
class Crop {
public:
    virtual ~Crop() {}
    virtual int steps() const = 0;
    void tend() const {
        for (int i = 0; i < steps(); i++) {
            harvest();
            move(North);
        }
    }
};

class Wheat : public Crop {
public:
    int steps() const { return 2; }
};

class Pumpkin : public Crop {
public:
    int steps() const { return 3; }
};

Crop* field[2];
field[0] = new Wheat();
field[1] = new Pumpkin();
for (int i = 0; i < 2; i++) {
    field[i]->tend();
    move(East);
    delete field[i];
}
