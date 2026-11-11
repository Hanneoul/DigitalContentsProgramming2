/* ==========================================================================
 * [C언어 파일 입출력의 완성: 꼭 알아야 할 핵심 함수 대백과]
 *
 * 지금까지 우리는 파일을 열고(`fopen`), 닫고(`fclose`), 문자열을 쓰고(`fprintf`),
 * 버퍼를 비우는(`fflush`) 기본적인 여정을 마쳤음. 하지만 C언어 표준 라이브러리에는
 * 이 외에도 파일의 운명을 바꿀 강력하고 다채로운 함수들이 아주 많이 존재함.
 *
 * 이번 파트에서는 실무와 학업에서 반드시 마주치게 될 핵심 입출력 함수들을 완벽하게 정리함.
 *
 * 1. 문자 단위 입출력: fgetc / fputc
 *    - fputc(int c, FILE *stream): 파일에 문자 하나(char)를 기록함.
 *    - fgetc(FILE *stream): 파일에서 문자 하나를 읽어옴. 파일 끝(EOF)에 도달하면 EOF(-1)를 반환함.
 *    - 용도: 파일 전체를 바이트 단위로 복사하거나, 특정 문자를 검색·변환할 때 주로 사용함.
 *
 * 2. 줄(Line) 단위 입출력: fgets / fputs
 *    - fputs(const char *str, FILE *stream): 문자열 한 줄을 파일에 기록함.
 *    - fgets(char *str, int n, FILE *stream): 파일에서 최대 n-1개의 문자를 읽어오거나, 개행 문자(`\n`)를
 *      만날 때까지 한 줄을 통째로 읽어옴. 버퍼 오버플로우를 막아주므로 안전함.
 *    - 용도: 텍스트 파일을 줄 단위로 읽어서 분석하거나 로그 파일을 기록할 때 필수적임.
 *
 * 3. 블록 단위 입출력 (바이너리의 꽃): fread / fwrite
 *    - size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream):
 *      메모리에 있는 데이터(구조체, 배열 등)의 바이트 덩어리를 통째로 디스크 파일에 밀어 넣음.
 *    - size_t fread(void *ptr, size_t size, size_t count, FILE *stream):
 *      디스크 파일에서 바이트 덩어리를 통째로 읽어와 메모리 변수에 곧바로 채워 넣음.
 *    - 용도: 게임 세이브 파일, 이미지/오디오 데이터, 대규모 구조체 배열을 고속으로 저장하고 불러올 때 사용함.
 *
 * 4. 파일 위치 제어 및 탐색: fseek / ftell / rewind
 *    - int fseek(FILE *stream, long offset, int origin): 파일 포인터(커서)의 위치를 원하는 곳으로 이동시킴.
 *      * SEEK_SET: 파일의 맨 처음 기준
 *      * SEEK_CUR: 현재 파일 포인터 기준
 *      * SEEK_END: 파일의 맨 끝(EOF) 기준
 *    - long ftell(FILE *stream): 파일의 시작점부터 현재 포인터까지 몇 바이트 떨어져 있는지 숫자로 반환함.
 *    - void rewind(FILE *stream): 파일 포인터를 무조건 맨 처음(SEEK_SET)으로 되돌림. (`fseek(fp, 0L, SEEK_SET)`과 동일)
 *    - 용도: 대용량 파일에서 특정 데이터를 임의 접근(Random Access)하거나 파일의 총 크기를 구할 때 사용함.
 *
 * 5. 파일 상태 검사 및 제어 함수들: feof / ferror / remove / rename
 *    - int feof(FILE *stream): 파일 읽기 도중 마지막 끝(EOF)에 도달했는지 확인함. (도달했으면 0이 아닌 값 반환)
 *    - int ferror(FILE *stream): 파일 연산 중 하드웨어 에러나 권한 에러가 발생했는지 확인함.
 *    - int remove(const char *filename): 지정한 파일을 하드디스크에서 영구히 삭제함.
 *    - int rename(const char *oldname, const char *newname): 파일의 이름을 변경하거나 다른 경로로 이동시킴.
 * ========================================================================== */
/* ==========================================================================
 * [C언어 파일 입출력의 완성: 꼭 알아야 할 핵심 함수 대백과 (fopen_s 버전)]
 *
 * 기존의 전통적인 `fopen` 함수는 파일 열기에 실패했을 때 단순히 `NULL`을 반환하는 것 외에
 * 에러에 대한 구체적인 정보를 제공하지 않거나, 잘못된 포인터 접근 시 보안상 취약할 수 있음.
 *
 * C11 표준부터 마이크로소프트(MSVC) 및 안전한 표준 라이브러리 환경에서는
 * 버퍼 오버플로우나 보안 취약점을 방지하기 위해 안전한 보안 버전인 `fopen_s`를 권장함.
 *
 * [fopen_s의 특징과 사용법]
 * - 형식: errno_t fopen_s(FILE **pStream, const char *filename, const char *mode);
 * - 첫 번째 인자로 `FILE *`의 주소(`&fp`)를 전달하고, 파일 열기 성공 여부는 반환값(errno_t, 성공 시 0)으로 확인함.
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>

  // 블록 단위 입출력(fread, fwrite) 실습을 위한 구조체
typedef struct 
{
    int id;
    char name[20];
} User;

int main(void) 
{
    FILE* fp = NULL;
    errno_t err;

    // --------------------------------------------------------------------------
    // [실습 1] fputc와 fgetc를 이용한 문자 단위 입출력
    // --------------------------------------------------------------------------
    err = fopen_s(&fp, "char_test.txt", "w");
    if (err != 0 || fp == NULL) return 1;

    // 알파벳 대문자 A부터 E까지 한 글자씩 파일에 기록한다.
    for (char c = 'A'; c <= 'E'; c++) {
        fputc(c, fp);
    }
    fclose(fp);
    printf("1단계: fputc를 이용한 문자 단위 쓰기 완료\n");

    // 파일을 읽기 모드로 열어 한 글자씩 읽어온다.
    err = fopen_s(&fp, "char_test.txt", "r");
    if (err != 0 || fp == NULL) return 1;

    printf("2단계: fgetc를 이용한 문자 읽기 결과 -> ");
    int ch;
    // fgetc는 파일을 읽다가 끝(EOF)에 도달하면 EOF(-1)를 반환한다.
    while ((ch = fgetc(fp)) != EOF) 
    {
        putchar(ch); // 화면에 한 글자씩 출력
    }
    printf("\n");
    fclose(fp);


    // --------------------------------------------------------------------------
    // [실습 2] fgets와 fputs를 이용한 줄(Line) 단위 입출력
    // --------------------------------------------------------------------------
    err = fopen_s(&fp, "line_test.txt", "w");
    if (err != 0 || fp == NULL) return 1;

    fputs("첫 번째 줄 안녕?\n", fp);
    fputs("두 번째 줄 반가워!\n", fp);
    fclose(fp);
    printf("3단계: fputs를 이용한 줄 단위 쓰기 완료\n");

    err = fopen_s(&fp, "line_test.txt", "r");
    if (err != 0 || fp == NULL) return 1;

    char line_buf[256];
    printf("4단계: fgets를 이용한 줄 단위 읽기 결과:\n");
    // 파일에서 한 줄씩 안전하게 읽어와 출력한다.
    while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        printf(" -> %s", line_buf);
    }
    fclose(fp);


    // --------------------------------------------------------------------------
    // [실습 3] fread와 fwrite를 이용한 블록 단위(구조체) 입출력
    // --------------------------------------------------------------------------
    User users[2] = {
        { 1, "Alice" },
        { 2, "Bob" }
    };

    FILE* bin_fp = NULL;
    // 바이너리 쓰기 모드로 구조체 배열 통째로 저장
    err = fopen_s(&bin_fp, "users.dat", "wb");
    if (err != 0 || bin_fp == NULL) return 1;

    // users 배열의 시작 주소부터, 구조체 크기(sizeof(User))만큼, 2개 요소를 파일에 쓴다.
    fwrite(users, sizeof(User), 2, bin_fp);
    fclose(bin_fp);
    printf("5단계: fwrite를 이용한 구조체 블록 저장 완료\n");

    // 바이너리 읽기 모드로 구조체 배열 통째로 복원
    err = fopen_s(&bin_fp, "users.dat", "rb");
    if (err != 0 || bin_fp == NULL) return 1;

    User loaded_users[2];
    // 디스크에서 데이터를 읽어와 loaded_users 배열에 곧바로 채운다.
    size_t read_count = fread(loaded_users, sizeof(User), 2, bin_fp);
    fclose(bin_fp);

    printf("6단계: fread로 복원된 데이터 확인 (읽어온 개수: %zu):\n", read_count);
    for (int i = 0; i < 2; i++) {
        printf(" -> ID: %d, Name: %s\n", loaded_users[i].id, loaded_users[i].name);
    }


    // --------------------------------------------------------------------------
    // [실습 4] fseek, ftell, rewind를 이용한 파일 위치 자유자재로 다루기
    // --------------------------------------------------------------------------
    err = fopen_s(&fp, "seek_test.txt", "w+");
    if (err != 0 || fp == NULL) return 1;

    fputs("ABCDEFGHIJ", fp); // 총 10바이트 기록

    // 현재 파일 포인터의 위치(바이트 오프셋)를 확인한다.
    printf("7단계 현재 위치(ftell): %ld 바이트\n", ftell(fp));

    // fseek를 이용해 파일 끝(SEEK_END)에서 앞으로 3바이트 이동한 위치로 커서를 옮긴다.
    fseek(fp, -3L, SEEK_END);
    printf("8단계 SEEK_END 기준 이동 후 위치(ftell): %ld 바이트\n", ftell(fp));

    // rewind를 이용해 포인터를 다시 맨 처음(0바이트)으로 강제 이동시킨다.
    rewind(fp);
    printf("9단계 rewind 후 위치(ftell): %ld 바이트\n", ftell(fp));

    fclose(fp);


    // --------------------------------------------------------------------------
    // [실습 5] feof와 ferror, 그리고 파일 삭제(remove)
    // --------------------------------------------------------------------------
    err = fopen_s(&fp, "char_test.txt", "r");
    if (err == 0 && fp != NULL) 
    {
        int temp;
        // 끝까지 읽을 때까지 반복
        while ((temp = fgetc(fp)) != EOF) 
        {
            // 내용 소모
        }

        // feof 검사: 정말로 파일의 끝(EOF)에 도달해서 끝난 것인지 확인
        if (feof(fp)) 
        {
            printf("10단계: feof 함수를 통해 파일의 끝에 정상적으로 도달했음을 감지함.\n");
        }
        fclose(fp);
    }

    // 테스트용으로 만들었던 파일들을 깔끔하게 삭제한다 (remove 함수)
    remove("char_test.txt");
    remove("line_test.txt");
    remove("users.dat");
    remove("seek_test.txt");
    printf("11단계: 생성했던 테스트 파일들 정리(remove) 완료\n");

    return 0;
}