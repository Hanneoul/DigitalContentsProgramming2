/* 게임 규칙은 Windows나 콘솔을 몰라도 실행할 수 있다. */
#ifndef ROAD_GAME_H // 중복 포함을 막는다.
#define ROAD_GAME_H // 헤더 읽음 표시이다.
enum { LANES = 3, TRAFFIC = 6 }; // 도로 차선과 재사용할 장애물 수이다.
typedef enum { READY, RUNNING, CRASHED, OVERHEATED, EMPTY } Phase; // 게임 생명주기이다.
typedef enum { REFUEL, HIT, BOOM, DRY } Event; // 화면에 전달할 사건이다.
typedef void (*EventCallback)(Event); // 사건을 받는 콜백 자료형이다.
typedef struct // 풀에 보관할 장애물 한 대이다.
{ // 장애물 필드 시작.
    int active, lane; // 활성 여부와 차선이다.
    float ahead; // 플레이어 앞쪽 거리이다.
} Obstacle; // 장애물 자료형 이름이다.
typedef struct // 게임 전체 상태이다.
{ // 게임 필드 시작.
    Phase phase; // 준비·주행·게임 오버 상태이다.
    int lane; // 플레이어의 현재 차선이다.
    float speed, heat, fuel; // 내부 계산용 값이며 화면에는 게이지만 보인다.
    float distance, nextFuel, nextTraffic; // 누적 거리와 다음 발생 지점이다.
    float steerDelay, flash; // 조향 반복 간격과 보급 효과 시간이다.
    unsigned int random; // 재현 가능한 장애물 배치용 난수 상태이다.
    Obstacle traffic[TRAFFIC]; // 메모리를 재사용하는 고정 크기 풀이다.
    EventCallback onEvent; // 표시 계층에서 등록하는 사건 처리 함수이다.
} Game; // 게임 상태 자료형이다.
void StartGame(Game *game, unsigned int seed, EventCallback callback); // 새 게임 초기화이다.
void UpdateGame(Game *game, float dt, int accelerate, int steer); // 한 틱을 진행한다.
#endif // 헤더 끝.
