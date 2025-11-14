#include <iostream>
#include <unistd.h>      // pentru pipe(), fork(), read(), write()
#include <sys/wait.h>    // pentru wait()
#include <cmath>

// Functie pentru verificarea numerelor prime
bool isPrime(int n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    int limit = (int) std::sqrt(n);
    for (int i = 3; i <= limit; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

int main() {
    const int N = 10000;
    const int NUM_PROCS = 10;
    const int INTERVAL = N / NUM_PROCS;

    // pipe-uri:
    // p2c[i] - parinte catre copil
    // c2p[i] - copil catre parinte
    int p2c[NUM_PROCS][2];
    int c2p[NUM_PROCS][2];

    // Cream pipe-urile pentru toate procesele copil
    for (int i = 0; i < NUM_PROCS; i++) {
        if (pipe(p2c[i]) == -1 || pipe(c2p[i]) == -1) {
            perror("Eroare creare pipe");
            return 1;
        }
    }

    for (int i = 0; i < NUM_PROCS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Eroare fork");
            return 1;
        }

        //          COD PROCES COPIL
        if (pid == 0) {

            // In copil, inchidem pipe-urile care nu ne apartin
            for (int j = 0; j < NUM_PROCS; j++) {
                if (j != i) {
                    close(p2c[j][0]);
                    close(p2c[j][1]);
                    close(c2p[j][0]);
                    close(c2p[j][1]);
                }
            }

            // Copilul va citi din p2c[i][0] si va scrie in c2p[i][1]
            close(p2c[i][1]); // copilul nu scrie catre parinte
            close(c2p[i][0]); // copilul nu citeste de la parinte

            int start, end;

            // Citirea intervalului
            if (read(p2c[i][0], &start, sizeof(start)) != sizeof(start))
                _exit(1);
            if (read(p2c[i][0], &end, sizeof(end)) != sizeof(end))
                _exit(1);

            close(p2c[i][0]); // am terminat de citit

            // Cautam numere prime in interval
            for (int x = start; x <= end; x++) {
                if (isPrime(x)) {
                    write(c2p[i][1], &x, sizeof(x));
                }
            }

            // Terminator 0
            int terminator = 0;
            write(c2p[i][1], &terminator, sizeof(terminator));
            close(c2p[i][1]);

            _exit(0); // copilul se opreste aici
        }

        //          COD PROCES PARINTE
        close(p2c[i][0]); // parintele nu citeste din pipe parinte->copil
        close(c2p[i][1]); // parintele nu scrie in pipe copil->parinte

        // Calculam intervalul fiecarui copil
        int start = i * INTERVAL + 1;
        int end   = (i + 1) * INTERVAL;
        if (i == NUM_PROCS - 1)
            end = N;

        // Trimitem intervalul copilului
        write(p2c[i][1], &start, sizeof(start));
        write(p2c[i][1], &end,   sizeof(end));

        close(p2c[i][1]); // s-a terminat scrierea catre copil
    }

    // Parintele citeste rezultatele
    std::cout << "Numere prime intre 1 si 10000:\n";

    for (int i = 0; i < NUM_PROCS; i++) {
        int number;
        while (true) {
            int r = read(c2p[i][0], &number, sizeof(number));

            if (r == 0) break;      // pipe inchis
            if (r < 0) break;       // eroare
            if (number == 0) break; // terminator de la copil

            std::cout << number << " ";
        }

        close(c2p[i][0]);
    }

    std::cout << "\n";

    // Asteptam procesele copil
    for (int i = 0; i < NUM_PROCS; i++)
        wait(nullptr);

    return 0;
}
