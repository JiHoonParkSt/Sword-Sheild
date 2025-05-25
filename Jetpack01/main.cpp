#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

using namespace std;

constexpr float PI = 3.14159265358979323846f;  // float용
constexpr double PI_D = 3.14159265358979323846; // double용

// 윈도우 크기
const int WIDTH = 1920;
const int HEIGHT = 1080;

// 플레이어 상태
float characterX = WIDTH / 2.0f;
float characterY = HEIGHT / 2.0f;

int playerHP = 100;
bool gameOver = false;

// 깜빡이는 상태
bool isBlinking = false;
float blinkTimer = 0.0f;
const float BLINK_DURATION = 3.0f;  // 3초 깜빡임

// 입력 상태
bool spacePressed = false;

// 마우스 좌표
double mouseX = 0, mouseY = 0;

// 방패 상태
struct Shield {
    float x, y;
    float angle;
    float radius;
};
Shield shield = { characterX + 60, characterY, 90.0f * PI / 180.0f, 40.0f };

//-------------------------------------추가코드---------------------------------------------//
// 기본 검을 지정하는 코드
struct Sword {
    float x, y;
    float angle;
    float radius;
};
Sword sword = { characterX + 60, characterY, 90.0f * PI / 180.0f, 40.0f };
//----------------------------------------------------------------------------------//

// 미사일 상태
struct Missile {
    float x, y; // 미사일의 위치
    float vx, vy; // 미사일의 속도
    float reflectCooldown = 0.0f; // 추가: 반사 쿨타임 (초)
};
std::vector<Missile> missiles;

// 입자
struct Particle {
    float x, y;
    float vx, vy;
    float life;
};
std::vector<Particle> particles;


// AABB 충돌 검사 함수
bool checkCollision(float x1, float y1, float w1, float h1,
    float x2, float y2, float w2, float h2) {
    return !(x1 + w1 < x2 || x1 > x2 + w2 ||
        y1 + h1 < y2 || y1 > y2 + h2);
}

// 마우스 이동
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    mouseX = xpos;
    mouseY = HEIGHT - ypos; // OpenGL 좌표 보정
}

// 미사일 생성
void createMissile() {
    Missile m;
    m.x = WIDTH; // 미사일이 오른쪽 끝에서 출발
    m.y = rand() % HEIGHT; // 랜덤한 y 좌표에서 시작
    m.vx = -3.0f; // 왼쪽으로 이동
    m.vy = rand() % 3 - 1; // 사선으로 랜덤한 속도
    missiles.push_back(m);
}


bool checkMissileShieldCollision(Missile& m) {
    if (m.reflectCooldown > 0.0f)
        return false; // 쿨타임 중이면 충돌 무시

    float dx = m.x - shield.x;
    float dy = m.y - shield.y;
    float distSq = dx * dx + dy * dy;

    if (distSq <= shield.radius * shield.radius) {
        // 미사일이 방패 범위 안에 들어옴

        // 방패의 바라보는 방향 벡터(?)
        float shieldDirX = cos(shield.angle);
        float shieldDirY = sin(shield.angle);

        // 미사일과 방패 중심 사이의 벡터
        float toMissileX = dx;
        float toMissileY = dy;

        // 방향 정규화
        float len = sqrt(toMissileX * toMissileX + toMissileY * toMissileY);
        if (len == 0) return false;

        toMissileX /= len;
        toMissileY /= len;

        // 방패 방향과 미사일 위치 벡터의 내적
        float dot = shieldDirX * toMissileX + shieldDirY * toMissileY;

        // dot > 0 이면 방패 앞쪽, dot < 0 이면 방패 뒷쪽
        if (dot > 0.0f) {
            return true; // 방패 앞쪽에서 맞았으면 반사
        }
    }
    return false;
}


void reflectMissile(Missile& m) {
    float dx = m.x - shield.x;
    float dy = m.y - shield.y;
    float len = sqrt(dx * dx + dy * dy);

    if (len == 0) return; // 방패 중심과 너무 가까우면 무시

    // 반사면의 법선 벡터 (방패 중심 → 미사일 방향)
    float nx = dx / len;
    float ny = dy / len;

    // 입사 벡터와 법선 벡터의 내적
    float dot = m.vx * nx + m.vy * ny;

    // 반사 공식: R = V - 2(N·V)N
    m.vx = m.vx - 2.0f * dot * nx;
    m.vy = m.vy - 2.0f * dot * ny;

    m.reflectCooldown = 1.0f; // 추가: 1초 쿨타임 설정
}


// 미사일 업데이트
void updateMissiles() {
    for (auto& m : missiles) {
        m.x += m.vx;
        m.y += m.vy;

        if (m.reflectCooldown > 0.0f) {
            m.reflectCooldown -= 1.0f / 60.0f; // 60FPS 기준
            if (m.reflectCooldown < 0.0f) m.reflectCooldown = 0.0f;
        }

        if (checkMissileShieldCollision(m)) {
            reflectMissile(m);
        }

        // 꼬리 입자 생성
        // 미사일 방향 벡터를 구함
        float len = std::sqrt(m.vx * m.vx + m.vy * m.vy);
        float dirX = m.vx / len;
        float dirY = m.vy / len;

        // 미사일 꼬리 위치 계산 (앞이 아닌 뒤)
        float tailX = m.x - dirX * 20.0f;
        float tailY = m.y - dirY * 20.0f;

        // 불꽃 입자 생성
        Particle p;
        p.x = tailX + (rand() % 6 - 3); // 살짝 퍼지게
        p.y = tailY + (rand() % 6 - 3);
        p.vx = (rand() % 10 - 5) * 0.1f; // 퍼지는 속도
        p.vy = (rand() % 10 - 5) * 0.1f;
        p.life = 0.5f;
        particles.push_back(p);
    }

    // 미사일이 화면 밖으로 나가면 삭제
    missiles.erase(std::remove_if(missiles.begin(), missiles.end(), [](Missile& m) {
        return (m.x < -50 || m.x > WIDTH + 50 || m.y < -50 || m.y > HEIGHT + 50);
        }), missiles.end());
}

// 입자 업데이트
void updateParticles() {
    for (auto& p : particles) {
        p.x += p.vx;
        p.y += p.vy;
        p.life -= 1.0f / 60.0f; // 프레임마다 생명 감소
    }

    // 죽은 입자 삭제
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](Particle& p) {
        return p.life <= 0.0f;
        }), particles.end());
}

// 방패 위치와 가리키는 방향 업데이트
void updateShield() {
    float dx = mouseX - characterX;
    float dy = mouseY - characterY;
    shield.angle = atan2(dy, dx);
    shield.x = characterX + cos(shield.angle) * 60.0f;
    shield.y = characterY + sin(shield.angle) * 60.0f;
}


//-------------------------------------추가 코드-----------------------------
// 칼 업데이트 - 방패와 같은 위치에 생성
void updateSword() {
    float dx = mouseX - characterX;
    float dy = mouseY - characterY;
    sword.angle = atan2(dy, dx);
    sword.x = characterX + cos(shield.angle) * 60.0f;
    sword.y = characterY + sin(shield.angle) * 60.0f;
}

// 플레이어 업데이트
void update() {
    if (spacePressed)
        characterY += 14.0f;
    else
        characterY -= 10.0f;

    if (characterY < 0) characterY = 0;
    if (characterY > HEIGHT) characterY = HEIGHT;

    // 미사일 충돌 처리 (깜빡이는 동안에는 충돌 체크 안 함)
    if (!isBlinking && !gameOver) {
        for (auto it = missiles.begin(); it != missiles.end(); ) {
            if (checkCollision(characterX - 25, characterY - 25, 50, 50,  // 플레이어 크기
                it->x - 10, it->y - 5, 20, 10)) {  // 미사일 크기

                playerHP -= 11; // HP 감소
                if (playerHP <= 0) {
                    playerHP = 0;
                    gameOver = true;
                }

                isBlinking = true;
                blinkTimer = BLINK_DURATION;
                it = missiles.erase(it); // 충돌한 미사일 삭제!
            }
            else {
                ++it; // 충돌 안 했으면 다음 미사일로 넘어감
            }
        }
    }

    // 깜빡이는 타이머
    if (isBlinking) {
        blinkTimer -= 1.0f / 60.0f; // 프레임당 감소
        if (blinkTimer <= 0.0f) {
            isBlinking = false;
            blinkTimer = 0.0f;
        }
    }

    updateSword(); // -------------추가 코드 : 칼 업데이트----------------
    updateShield();
}

void drawText(const char* text, float x, float y, float r, float g, float b, float scale = 2.0f) {
    char buffer[99999]; // 충분히 크게
    int num_quads;

    glPushMatrix(); // 현재 매트릭스 저장
    glTranslatef(x, y, 0); // (x, y) 위치로 이동
    glScalef(scale, scale, 1.0f); // 글자 크기 확대
    glScalef(1.0f, -1.0f, 1.0f); // Y축 뒤집기

    glColor3f(r, g, b);
    num_quads = stb_easy_font_print(0, 0, (char*)text, NULL, buffer, sizeof(buffer));

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 16, buffer);
    glDrawArrays(GL_QUADS, 0, num_quads * 4);
    glDisableClientState(GL_VERTEX_ARRAY);

    glPopMatrix(); // 원래 매트릭스로 복원
}

void renderHPBar() {
    float barWidth = 200.0f;
    float barHeight = 20.0f;
    float x = 20.0f;
    float y = HEIGHT - 70.0f; // 바 위치
    float textOffsetY = 50.0f; // 텍스트를 바보다 위로 띄우기

    // 1. HP 텍스트 ("HP" + 수치)
    char hpText[32];
    snprintf(hpText, sizeof(hpText), "HP: %d", playerHP);
    drawText(hpText, x, y + textOffsetY, 1.0f, 1.0f, 1.0f); // 흰색 글자

    // 2. HP Bar 배경 (회색)
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + barWidth, y);
    glVertex2f(x + barWidth, y + barHeight);
    glVertex2f(x, y + barHeight);
    glEnd();

    // 3. 현재 HP에 비례하는 채운 부분 (초록)
    float hpRatio = static_cast<float>(playerHP) / 100.0f;
    glColor3f(0.0f, 1.0f, 0.0f); // 나중에 hpRatio에 따라 색 바꿔줄 수도 있어
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + barWidth * hpRatio, y);
    glVertex2f(x + barWidth * hpRatio, y + barHeight);
    glVertex2f(x, y + barHeight);
    glEnd();
}

// 입자 렌더링
void renderParticles() {
    for (auto& p : particles) {
        float alpha = p.life * 2.0f;
        glColor4f(1.0f, 0.5f + 0.5f * (p.life), 0.0f, alpha);

        glBegin(GL_QUADS);
        glVertex2f(p.x - 2, p.y - 2);
        glVertex2f(p.x + 2, p.y - 2);
        glVertex2f(p.x + 2, p.y + 2);
        glVertex2f(p.x - 2, p.y + 2);
        glEnd();
    }
}

// 방패 렌더링
void renderShield() {
    int numSegments = 20;
    float angleStep = PI / numSegments;

    glColor3f(0.0f, 1.0f, 1.0f); // 청록색

    // 중심각 shield.angle를 기준으로 -90도 ~ +90도 범위로 포인트를 찍어 나간다.
    // 즉, 방패의 반원을 그려주는 코드
    glBegin(GL_LINE_STRIP);
    for (int i = -numSegments / 2; i <= numSegments / 2; i++) {
        float angle = shield.angle + i * angleStep;
        float x = shield.x + cos(angle) * shield.radius; // cos, sin은 라디안을 매개변수로 받는다.
        float y = shield.y + sin(angle) * shield.radius;
        glVertex2f(x, y);
    }
    glEnd();

    // 하이픈 2줄
    float offset = 10.0f;
    float dirX = cos(shield.angle);
    float dirY = sin(shield.angle);
    float sideX = -dirY;
    float sideY = dirX;

    for (float o : { -offset, offset }) {
        glBegin(GL_LINES);
        glVertex2f(shield.x + sideX * o, shield.y + sideY * o);
        glVertex2f(shield.x + sideX * o + dirX * 20.0f, shield.y + sideY * o + dirY * 20.0f);
        glEnd();
    }
}

//---------------------------------------추가 코드------------------------
// 칼을 그리는 코드
void renderSword() {
    // 칼날 (회색)
    glColor3f(0.8f, 0.8f, 0.9f);
    glBegin(GL_QUADS);
    glVertex2f(sword.x - 8, sword.y);
    glVertex2f(sword.x + 8, sword.y);
    glVertex2f(sword.x + 6, sword.y + 110);
    glVertex2f(sword.x - 6, sword.y + 110);
    glEnd();

    // 손잡이 (진한 갈색)
    glColor3f(0.4f, 0.2f, 0.1f);
    glBegin(GL_QUADS);
    glVertex2f(sword.x - 7, sword.y - 25);
    glVertex2f(sword.x + 7, sword.y - 25);
    glVertex2f(sword.x + 7, sword.y);
    glVertex2f(sword.x - 7, sword.y);
    glEnd();

    // 손잡이 끝 장식 (노란색)
    glColor3f(0.9f, 0.85f, 0.2f);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 18; ++i) {
        float theta = 2.0f * 3.1415926f * i / 18;
        float dx = 7 * cos(theta);
        float dy = 5 * sin(theta);
        glVertex2f(sword.x + dx, sword.y - 30 + dy);
    }
    glEnd();

    // 칼자루 가드 (십자, 노란색)
    glColor3f(0.9f, 0.85f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(sword.x - 20, sword.y - 3);
    glVertex2f(sword.x + 20, sword.y - 3);
    glVertex2f(sword.x + 20, sword.y + 3);
    glVertex2f(sword.x - 20, sword.y + 3);
    glEnd();
}



//-------------------------------------추가코드---------------------------------------------//
// 현재 아무것도 들고 있지 않을 때 0
int currentWeapon = 0;
//---------------------------------------------------------------------------------//

// 키 입력
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_SPACE) {
        if (action == GLFW_PRESS)
            spacePressed = true;
        else if (action == GLFW_RELEASE)
            spacePressed = false;
    }
    //-------------------------------------추가코드---------------------------------------------//
    // 1번을 누르면 상태 1, 2번을 누르면 상태 2로 변경됨
        if (key == GLFW_KEY_1) {
            currentWeapon = 1;
        }
        else if (key == GLFW_KEY_2) {
            currentWeapon = 2;
        }
    }
    //----------------------------------------------------------------------------------//

// 렌더링
void render() {
    glClear(GL_COLOR_BUFFER_BIT);

    renderParticles();

    // 플레이어 그리기 (깜빡이는 동안에는 반투명)
    if (isBlinking) {
        float alpha = (std::sin(blinkTimer * 10.0f) * 0.5f + 0.5f) * 0.5f + 0.5f;
        glColor4f(1.0f, 0.0f, 0.0f, alpha);
    }
    else {
        glColor3f(1.0f, 0.5f, 0.0f); // 기본 색깔
    }
    //glColor3f(1.0f, 0.5f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(characterX - 25, characterY - 25);
    glVertex2f(characterX + 25, characterY - 25);
    glVertex2f(characterX + 25, characterY + 25);
    glVertex2f(characterX - 25, characterY + 25);
    glEnd();

    //-------------------------------------추가코드---------------------------------------------//
    // 만약 현재 상태가 1(1을 눌렀을 때) 검을 들고, 현재 상태가 2(2를 눌렀을 때) 방패를 드는 코드
    if (currentWeapon == 1)
    {
        renderSword();
    }
    else if (currentWeapon == 2)
    {
        renderShield();
    }
    //----------------------------------------------------------------------------------//

    // 미사일
    for (auto& m : missiles) {
        glPushMatrix(); // 현재 좌표계 저장

        // 미사일 위치로 이동
        glTranslatef(m.x, m.y, 0.0f);

        // 미사일 회전 (방향에 맞춰)
        float angle = atan2(m.vy, m.vx) * 180.0f / 3.141592f;
        glRotatef(angle + 180, 0.0f, 0.0f, 1.0f); // 회전 보정

        // 미사일 몸통 (회색)
        glColor3f(0.7f, 0.7f, 0.7f);
        glBegin(GL_QUADS);
        glVertex2f(-20, -5); // -20
        glVertex2f(0, -5);
        glVertex2f(0, 5);
        glVertex2f(-20, 5); // - 20
        glEnd();

        // 미사일 머리 (빨간 삼각형)
        glColor3f(1.0f, 0.0f, 0.0f);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, 0);
        glVertex2f(10, 7);
        glVertex2f(10, -7);
        glEnd();

        glPopMatrix(); // 원래 좌표계 복구
    }

    renderHPBar();
}


// 메인
int main() {
    srand(static_cast<unsigned int>(time(0)));

    if (!glfwInit()) {
        std::cout << "GLFW 초기화 실패!" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Shield Reflect Game", nullptr, nullptr);
    if (!window) {
        std::cout << "윈도우 생성 실패!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, 0, HEIGHT, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    double lastTime = glfwGetTime();
    double missileCooldown = 1.0;

    while (!glfwWindowShouldClose(window)) {
        if (gameOver) {
            std::cout << "게임 오버! 플레이어가 사망했습니다." << std::endl;
            break;
        }

        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= missileCooldown) {
            createMissile();
            lastTime = currentTime;
        }

        update();
        updateMissiles();
        updateParticles();
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
