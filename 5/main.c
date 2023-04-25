#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>

sem_t *semaphore;
int shmid;
int *count_array;

void studentess_process(sem_t *semaphore, int fans_count, int *shared_array) {
    int mx = 0, mx_id = -1;
    for (int i = 0; i < fans_count; ++i) {
        sem_wait(semaphore);
    }
    printf("Просмотр открыток начат ...\n");
    for (int i = 0; i < fans_count; ++i) {
        if (shared_array[i] > mx) {
            mx = shared_array[i];
            mx_id = i;
        }
    }
    printf("Просмотр открыток завершен\n");

    if (mx_id != -1) {
        printf("Самое привлекательное предложение — от фаната номер %d (%d привлекательности).\n"
               "Студентка выбирает его.\n", mx_id + 1, mx);
    } else {
        printf("Никто из фанатов не заинтересовал студентку.\n");
    }
}


void fan_process(sem_t *semaphore, int num) {
    srand(time(NULL) * num);

    int date_attractiveness = rand() % 100;
    count_array[num] = date_attractiveness;

    printf("Фанат %d прислал открытку, его предложение привлекательно на %d процентов \n", num + 1,
           date_attractiveness);
    sem_post(semaphore);
}

// функция для обработки прерывания
void sigintHandler(int signum) {
    int status;
    pid_t pid;

    if ((pid = waitpid(-1, &status, 0)) < 0) {
        // Ошибка: это дочерний процесс
    } else {
        printf("Получени сигнал ПРЕРЫВАНИЯ.\n");

        // Зачистка памяти:
        sem_close(semaphore);
        sem_unlink("fans_semaphore");
        munmap(count_array, sizeof(count_array));

        printf("Свидание отменено пользователем.\n");
    }

    exit(signum);
}

int main(int argc, char **argv) {
    // Обработка входных данных:
    if (argc != 2) {
        printf("Использование: ./main <fan count>\n");
        return -1;
    }
    int fans_count = atoi(argv[1]);
    if (fans_count < 1 || fans_count > 10000) {
        printf("Ошибка: количество фанатов должно быть от 1 до 10000 включительно\n");
        exit(EXIT_FAILURE);
    }

    // Подготовка к получению сигнала ПРЕРЫВАНИЯ:
    (void) signal(SIGINT, sigintHandler);

    // Подготовка общей памяти:
    count_array = mmap(NULL, sizeof(int) * fans_count,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS,
                       -1,
                       0);

    // Подготовка семафоров:
    semaphore = sem_open("fans_semaphore", O_CREAT, 0644, 0);
    if (semaphore == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    // Запуск процессов-поклонников:
    for (int i = 0; i < fans_count; ++i) {
        pid_t pid;
        if ((pid = fork()) < 0) {
            perror("Ошибка при создании процесса:\n");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            fan_process(semaphore, i);
            exit(EXIT_SUCCESS);
        }
    }

    // Запуск процесса-студентки:
    studentess_process(semaphore, fans_count, count_array);

    // Зачистка памяти:
    sem_close(semaphore);
    sem_unlink("fans_semaphore");
    munmap(count_array, sizeof(count_array));

    // Завершение программы:
    printf("День завершён.\n");
    return 0;
}