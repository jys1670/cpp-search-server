// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:

// Закомитьте изменения и отправьте их в свой репозиторий.

#include <iostream>

using namespace std;

int main() {
    int i = 0, j = 0, k = 0, counter = 0;
    // [0, 1000)
    while (i < 10) {
        while (j < 10) {
            while (k < 10) {
                if (i == 3 || j == 3 || k == 3) {
                    ++counter;
                }
                ++k;
            }
            ++j;
            k = 0;
        }
        ++i;
        j = 0;
    }
    cout << to_string(counter) << endl;
    return 0;
}