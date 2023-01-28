// Simple ds18b20 class
class DS18B20
{
public:
    DS18B20(int gpio)
    {
        pin = gpio;
    }

    float temp()
    {
        uint8_t arr[2] = {0};
        if (reset()) {
            wByte(0xCC);
            wByte(0x44);
            delay(750);
            reset();
            wByte(0xCC);
            wByte(0xBE);
            arr[0] = rByte();
            arr[1] = rByte();
            reset();
            return (float)(arr[0] + (arr[1] * 256)) / 16;
        }
        return 0;
    }
private:
    int pin;

    void write(uint8_t bit)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delayMicroseconds(5);
        if (bit)digitalWrite(pin, HIGH);
        delayMicroseconds(80);
        digitalWrite(pin, HIGH);
    }

    uint8_t read()
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delayMicroseconds(2);
        digitalWrite(pin, HIGH);
        delayMicroseconds(15);
        pinMode(pin, INPUT);
        return digitalRead(pin);
    }

    void wByte(uint8_t bytes)
    {
        for (int i = 0; i < 8; ++i) {
            write((bytes >> i) & 1);
        }
        delayMicroseconds(100);
    }

    uint8_t rByte()
    {
        uint8_t r = 0;
        for (int i = 0; i < 8; ++i) {
            if (read()) r |= 1 << i;
            delayMicroseconds(15);
        }
        return r;
    }

    bool reset()
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        delayMicroseconds(500);
        digitalWrite(pin, HIGH);
        pinMode(pin, INPUT);
        delayMicroseconds(500);
        return digitalRead(pin);
    }
};
