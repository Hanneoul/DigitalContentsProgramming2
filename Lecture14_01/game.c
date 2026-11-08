/* 규칙 읽기 순서: StartGame → UpdateGame → Spawn / EndGame. */
#include "game.h" // 게임 상태와 함수 선언이다.

static float Clamp(float value, float low, float high) // 범위를 넘는 게이지를 제한한다.
{ // 범위 검사 시작.
    return value < low ? low : value > high ? high : value; // 양 끝 사이의 값이다.
} // 범위 검사 끝.

static void Emit(Game *game, Event event) // 사건이 있을 때만 등록된 반응을 부른다.
{ // 사건 전달 시작.
    if (game->onEvent != 0) game->onEvent(event); // 콜백이 없으면 생략한다.
} // 사건 전달 끝.

static void EndGame(Game *game, Phase phase, Event event) // 실패를 한 번 확정한다.
{ // 종료 처리 시작.
    game->phase = phase; // 실패 원인을 상태로 남긴다.
    game->speed = 0; // 게임 오버 뒤에는 더 달리지 못한다.
    Emit(game, event); // 충돌·폭발·연료 소진을 알린다.
} // 종료 처리 끝.

void StartGame(Game *game, unsigned int seed, EventCallback callback) // 재시작도 같은 초기화를 쓴다.
{ // 초기화 시작.
    Game fresh = {0}; // 풀을 포함한 모든 값을 초기화한다.
    fresh.phase = RUNNING; // 주행 상태로 전환한다.
    fresh.lane = 1; // 가운데 차선에서 출발한다.
    fresh.fuel = 1; // 연료는 가득 찬 상태이다.
    fresh.nextFuel = 420; // 첫 보급선의 월드 거리이다.
    fresh.nextTraffic = 45; // 출발 직후에는 장애물을 바로 만들지 않는다.
    fresh.random = seed; // 난수 시작값을 기록한다.
    fresh.onEvent = callback; // 화면의 사건 처리기를 등록한다.
    *game = fresh; // 새 상태를 한 번에 적용한다.
} // 초기화 끝.

static void Spawn(Game *game) // 빈 풀 칸에 장애물 한 대를 생성한다.
{ // 생성 시작.
    for (int i = 0; i < TRAFFIC; ++i) // 풀의 모든 칸을 검사한다.
    { // 슬롯 검사 시작.
        if (game->traffic[i].active) continue; // 활성 객체는 덮어쓰지 않는다.
        game->random = game->random * 1664525u + 1013904223u; // unsigned 순환을 이용한 간단한 난수이다.
        game->traffic[i].active = 1; // 게임에 참여시킨다.
        game->traffic[i].lane = (int)((game->random >> 16) % LANES); // 차선 하나를 선택한다.
        game->traffic[i].ahead = 100; // 화면 위쪽에서 나타난다.
        return; // 한 번에 한 대만 생성한다.
    } // 슬롯 검사 끝.
} // 생성 끝.

void UpdateGame(Game *game, float dt, int accelerate, int steer) // Input 뒤에 호출되는 Update이다.
{ // 한 틱 시작.
    if (game->phase != RUNNING || dt <= 0) return; // 종료 화면에서는 시간이 흐르지 않는다.
    dt = Clamp(dt, 0, 0.05f); // 긴 정지 뒤 한 번에 멀리 이동하지 않도록 제한한다.
    game->flash = Clamp(game->flash - dt, 0, 1); // 보급 효과를 서서히 지운다.
    game->steerDelay -= dt; // 다음 차선 이동까지의 간격을 줄인다.
    if (steer == 0) game->steerDelay = 0; // 키를 놓으면 다음 누름은 즉시 반응한다.
    if (steer != 0 && game->steerDelay <= 0) // 좌우 입력은 일정 간격으로만 이동한다.
    { // 조향 시작.
        game->lane += steer < 0 ? -1 : 1; // 차선 하나만 옮긴다.
        if (game->lane < 0) game->lane = 0; // 왼쪽 도로 밖으로 나가지 않는다.
        if (game->lane >= LANES) game->lane = LANES - 1; // 오른쪽 경계이다.
        game->steerDelay = 0.18f; // 길게 눌러도 순간적으로 끝 차선에 가지 않는다.
    } // 조향 끝.
    game->speed = Clamp(game->speed + (accelerate ? 0.50f : -0.28f) * dt, 0, 1); // 가속 또는 관성 감속이다.
    game->heat = Clamp(game->heat + (accelerate ? 0.095f : -0.24f) * dt, 0, 1); // 페달을 놓으면 냉각한다.
    game->fuel = Clamp(game->fuel - dt / 18.0f, 0, 1); // 속도와 관계없이 제한 시간이 줄어든다.
    if (game->heat >= 1) // 완전히 과열되면 보급선보다 먼저 폭발 판정한다.
    { // 과열 처리 시작.
        EndGame(game, OVERHEATED, BOOM); // 열 게이지는 가득 찬 채로 남는다.
        return; // 더 이동하지 않는다.
    } // 과열 처리 끝.
    if (game->fuel <= 0) // 제한 시간이 끝나면 멈춘다.
    { // 연료 소진 시작.
        EndGame(game, EMPTY, DRY); // 거리 점수를 확정한다.
        return; // 종료된 틱에는 보급받지 못한다.
    } // 연료 소진 끝.
    float travel = game->speed * 48.0f * dt; // 이번 틱의 전진 거리이다.
    game->distance += travel; // 유일한 숫자 점수에 반영한다.
    for (int i = 0; i < TRAFFIC; ++i) // 모든 활성 장애물을 갱신한다.
    { // 장애물 갱신 시작.
        Obstacle *object = &game->traffic[i]; // 원본 객체의 주소이다.
        if (!object->active) continue; // 비활성 객체는 건너뛴다.
        float before = object->ahead; // 빠른 이동으로 충돌 구간을 건너뛰는지 확인한다.
        object->ahead -= travel; // 플레이어가 전진한 만큼 아래로 다가온다.
        if (object->lane == game->lane && before >= -12 && object->ahead <= 12) // 블록 차체 길이를 포함한 충돌이다.
        { // 충돌 처리 시작.
            EndGame(game, CRASHED, HIT); // 충돌 사건을 알리고 멈춘다.
            return; // 뒤쪽 객체를 더 갱신하지 않는다.
        } // 충돌 처리 끝.
        if (object->ahead < -14) object->active = 0; // 지나간 차량의 칸을 반납한다.
    } // 장애물 갱신 끝.
    if (game->distance >= game->nextFuel) // 도로 전체에 걸친 보급선을 통과했다.
    { // 보급 시작.
        game->fuel = 1; // 다음 제한 시간을 가득 채운다.
        game->nextFuel += 420; // 다음 보급선은 같은 간격으로 배치한다.
        game->flash = 1; // 잠깐 보급 성공을 강조한다.
        Emit(game, REFUEL); // 표시용 콜백이다. 엔진 열은 초기화하지 않는다.
    } // 보급 끝.
    if (game->distance >= game->nextTraffic) // 시간 대신 이동 거리에 따라 장애물이 나온다.
    { // 생성 예약 처리 시작.
        Spawn(game); // 빈 칸을 재사용한다.
        game->nextTraffic += 36; // 충분한 회피 거리를 두어 전 차선 벽을 만들지 않는다.
    } // 생성 예약 처리 끝.
} // 한 틱 끝.
