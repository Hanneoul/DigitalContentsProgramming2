#include "player_api.h"

/* 화면에 표시할 자기 이름을 입력하세요. */
char player3_name[PLAYER_NAME_BYTES] = u8"Player3";

/* 기본 예제: 상태 파일을 읽고 상/하/좌/우 이동을 차례로 선택합니다. */
Command player3_choose_command(const char *state_path)
{
    Snapshot state;
    FILE *file = fopen(state_path, "r");
    int loaded;

    if (file == NULL)
        return COMMAND_UP;

    loaded = snapshot_read(file, &state);
    fclose(file); /* 읽기에 실패해도 열린 파일은 반드시 닫습니다. */

    if (!loaded)
        return COMMAND_UP;

    /* 여기에 자신의 판단 코드를 작성하세요.
       현재 예제는 벽, 상대, 열쇠를 판단하지 않고 이동 방향만 순환합니다. */
    return (Command)((state.turn + state.self) % 4);
}
