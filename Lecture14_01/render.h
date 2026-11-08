#ifndef ROAD_RENDER_H // 중복 선언 방지이다.
#define ROAD_RENDER_H // 헤더 표시이다.
#include "game.h" // 표시할 게임 상태이다.
void Render(const Game *game); // 게임 상태를 바꾸지 않고 그린다.
void OnGameEvent(Event event); // 게임에서 등록할 표시용 콜백이다.
#endif // 헤더 끝.
