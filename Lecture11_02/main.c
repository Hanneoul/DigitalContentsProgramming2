/*
==============================================================================
 [강의 자료] Lecture11_02 | GameObject | 데이터에 생명주기 붙이기
==============================================================================

1. 같은 게임을 한 겹씩 확장한다
    01: 입력 → 이동 → 화면
    02: GameObject + Start / Update / Destroy
    03: 객체 하나 → 고정 배열 Pool
    04: 도착 사실 → OnGoal 이벤트 → 점수
    이전 프로젝트를 복사한 뒤 [추가] 부분을 붙인다. 각 완성본은 독립 실행된다.
    표시용 panel, track과 입력용 command는 제공 코드(console.h)이다.

 2. 게임 루프의 세 박자
       Input(입력) ---> Update(상태 변경) ---> Render(그리기)
           ^                                      |
           +--------------------------------------+
    Loop는 고리, Frame은 한 장의 화면이다. 한 바퀴에 상태를 한 번 바꾼다.
    이 수업은 Enter로 한 틱씩 진행하는 턴 방식이다. Tick은 시계의 한 눈금이다.
    실시간 게임은 입력을 기다리지 않고 반복하며 경과 시간도 고려해야 한다.
    아래 코드는 실시간 프레임 속도나 delta time(경과 시간)을 구현하지 않는다.
    Enter=진행, q=종료. 입력 파일 끝(EOF)도 종료로 처리한다.

 3. 01에서 달라지는 부분
    int x → GameObject hero. 이동 한 줄 → Update(&hero).
    Object는 다루는 대상, GameObject는 게임 속 대상을 뜻한다.
    C에 내장된 엔진 클래스가 아니다. 위치와 활성 여부를 묶은 우리 자료형이다.
    Lifecycle은 life(생명) + cycle(주기)이다.
       Start 1회 → Update 여러 번 → Destroy 1회 → 비활성
    여기서 Destroy는 free가 아니다. 지역 구조체의 메모리는 main이 끝날 때까지 있다.
    실험: Enter 10번에는 @가 보이고 11번에는 사라진다.
    확인: Start에서 x=0을 빼면 재시작 위치가 초기화되지 않는다.

==============================================================================
 [실습 코드] 먼저 실행 → 예측 → 추가 부분 작성 → 다시 실행
==============================================================================
*/

#include "../LectureSupport/console.h" // 제공된 화면과 입력이다.

typedef struct // [추가] struct는 서로 관련된 값의 구조를 정의한다.
{ // GameObject의 필드 시작.
    int x; // 위치이다.
    int active; // 활성 여부: 1이면 게임에 참여한다.
} GameObject; // typedef로 자료형에 짧은 이름을 붙인다.

void Start(GameObject *object) // [추가] 한 번의 활성화가 시작될 때 호출한다.
{ // Start 시작.
    object->x = 0; // 재사용할 때도 출발점으로 초기화한다.
    object->active = 1; // 게임에 참여시킨다.
} // Start 끝.

void Destroy(GameObject *object) // [추가] 게임 참여를 끝낸다.
{ // Destroy 시작.
    object->active = 0; // 메모리를 해제하지 않고 비활성으로 표시한다.
} // Destroy 끝.

void Update(GameObject *object) // [변경] 변수 대신 객체의 주소를 받는다.
{ // Update 시작.
    if (!object->active) return; // 비활성 객체는 움직이지 않는다.
    ++object->x; // ->는 포인터가 가리키는 구조체의 필드에 접근한다.
    if (object->x >= 11) Destroy(object); // 도착한 객체를 비활성화한다.
} // Update 끝.

int main(void) // 같은 게임 루프를 유지한다.
{ // main 시작.
    GameObject hero = {0, 0}; // [변경] 위치와 활성 여부를 한 객체에 묶는다.
    Start(&hero); // &는 주소이다. 시작 함수가 원본을 수정한다.
    panel("11-02 | GAME OBJECT / LIFECYCLE"); // 제목이다.
    track(hero.x, hero.active); // .은 실제 구조체의 필드 접근이다.
    puts("  Enter: step | q: quit"); // 조작법이다.
    while (hero.active) // 활성 상태인 동안 게임을 진행한다.
    { // 한 틱 시작.
        int key = command(); // Input이다.
        if (key == 'q' || key == EOF) break; // 종료를 처리한다.
        if (key != '\n') continue; // Enter만 진행한다.
        Update(&hero); // Update: 원본 객체를 이동시킨다.
        track(hero.x, hero.active); // Render: 비활성이면 경로에서 사라진다.
    } // 루프 끝.
    puts(hero.x >= 11 ? "  CLEAR! Destroy -> inactive" : "  Run ended."); // 결과이다.
    Destroy(&hero); // 중도 종료 시에도 참여를 끝낸다.
    return 0; // 정상 종료이다.
} // main 끝.
