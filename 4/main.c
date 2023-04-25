#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int fans_id;
int shmid;
int *count_array;

int getSemaphoreSet(int cnt_sems, int sem_key) {
    int sem_id = semget(sem_key, cnt_sems, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("Ошибка при создании набора семафоров: ");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < cnt_sems; ++i) {
        if (semctl(sem_id, i, SETVAL, 0) < 0) {
            perror("Ошибка при создании семафора: ");
            exit(EXIT_FAILURE);
        }
    }

    printf("    [Набор из %d семафоров создан. Id: %d]\n", cnt_sems, sem_id);
    return sem_id;
}

void eraseSemaphore(int sem_id) {
    printf("    [Удаление набора семафоров с id: %d]\n", sem_id);
    if (semctl(sem_id, 0, IPC_RMID) < 0) {
        perror("Ошибка удаления набора семафоров: ");
        exit(EXIT_FAILURE);
    }
}

void runOp(int sem_id, int sem_num, int sem_op, int sem_flg) {
    struct sembuf sb;

    sb.sem_num = sem_num;
    sb.sem_op = sem_op;
    sb.sem_flg = sem_flg;

    if (semop(sem_id, &sb, 1) < 0) {
        perror("Ошибка выполнения semop: ");
        exit(EXIT_FAILURE);
    }
}

void studentess_process(int fans_id, int fans_count, int *shared_array) {
    printf("Просмотр открыток начат ...\n");
    int mx = 0, mx_id = -1;
    for (int i = 0; i < fans_count; ++i) {
        runOp(fans_id, i, -1, 0);
        //printf("%d ", shared_array[i]);
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


void fan_process(int sem_id, int num, int *count_array) {
    bool is_done = false;
    srand(time(NULL) * num);

    int date_attractiveness = rand() % 100;
    count_array[num] = date_attractiveness;

    printf("Фанат %d прислал открытку, его предложение привлекательно на %d процентов \n", num + 1,
           date_attractiveness);

    runOp(sem_id, num, 1, 0);
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

        eraseSemaphore(fans_id);

        shmdt(count_array);
        shmctl(shmid, IPC_RMID, NULL);

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
    key_t shm_key = ftok(argv[0], 0);
    if ((shmid = shmget(shm_key, sizeof(int) * fans_count, 0666 | IPC_CREAT | IPC_EXCL)) < 0) {
        if ((shmid = shmget(shm_key, sizeof(int) * fans_count, 0)) < 0) {
            printf("Не удаётся создать общую память:\n");
            exit(EXIT_FAILURE);
        };
        count_array = (int *) shmat(shmid, NULL, 0);
        printf("Общая память найдена.\n");
    } else {
        count_array = (int *) shmat(shmid, NULL, 0);
        for (int i = 0; i < fans_count; ++i) {
            count_array[i] = 0;
        }
        printf("Общая память создана.\n");
    }

    // Подготовка семафоров:
    fans_id = getSemaphoreSet(fans_count, rand() % 10000);

    // Запуск процессов-поклонников:
    for (int i = 0; i < fans_count; ++i) {
        pid_t pid;
        if ((pid = fork()) < 0) {
            perror("Ошибка при создании процесса:\n");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            fan_process(fans_id, i, count_array);
            exit(EXIT_SUCCESS);
        }
    }

    // Запуск процесса-студентки:
    studentess_process(fans_id, fans_count, count_array);

    // Зачистка памяти:
    eraseSemaphore(fans_id);
    shmdt(count_array);
    shmctl(shmid, IPC_RMID, NULL);

    // Завершение программы:
    printf("День завершён.\n");
    return 0;
}